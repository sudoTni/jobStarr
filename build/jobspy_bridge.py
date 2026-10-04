#!/usr/bin/env python3
"""
jobspy_bridge.py - JobSpy Python adapter for jobStarr (jS)

Developed by the jobStarr Contributors.
Powered by the JobSpy scraping engine (https://github.com/speedyapply/JobSpy/),
bundled in third_party/jobspy/ — see third_party/README.md.

Invoked by the C++ jobStarr application via QProcess:
    python3 jobspy_bridge.py --url "<job-url>"

Requires the JobSpy runtime dependencies:
    pip install -r tools/requirements-jobspy.txt

Contract:
- On success: exit code 0, writes ONLY valid UTF-8 JSON to stdout.
- On failure: exit code non-zero, stdout empty, error message on stderr.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from datetime import date, datetime, timedelta
from typing import Any, Optional
from urllib.parse import parse_qs, urlparse, urlunparse


def discover_and_load_jobspy() -> None:
    """Locates and puts on sys.path the directory containing the `jobspy` package.

    jobStarr bundles JobSpy in `third_party/jobspy/`, which the build copies next
    to this script, so the common case needs no external checkout. Resolution
    order (first match wins):

      1. $JOBSTARR_JOBSPY_PATH                     - explicit override
      2. <script_dir>                              - bundled copy beside the script
      3. <script_dir>/../third_party               - bundled copy in the source tree
      4. any ancestor's third_party/               - bundled copy, walking upwards
      5. legacy jobStarr-res/JobSpy sibling layouts (deprecated, kept for
         developers who already keep a separate checkout)
    """
    candidates = []

    # 1. Environment variable override
    env_path = os.environ.get("JOBSTARR_JOBSPY_PATH")
    if env_path:
        candidates.append(os.path.abspath(env_path))

    script_dir = os.path.dirname(os.path.abspath(__file__))

    # 2. Bundled copy deployed next to this script (CMake POST_BUILD)
    candidates.append(script_dir)

    # 3. Bundled copy in the source tree
    candidates.append(os.path.abspath(os.path.join(script_dir, "..", "third_party")))

    # 4. Bundled copy, walking up from the script (covers out-of-tree build dirs)
    walk = script_dir
    for _ in range(4):
        parent = os.path.dirname(walk)
        if not parent or parent == walk:
            break
        walk = parent
        candidates.append(os.path.abspath(os.path.join(walk, "third_party")))

    # 5. Legacy sibling-repository layouts (deprecated)
    for base in (script_dir, os.getcwd()):
        candidates.append(os.path.abspath(os.path.join(base, "jobStarr-res", "JobSpy")))
        candidates.append(os.path.abspath(os.path.join(base, "..", "jobStarr-res", "JobSpy")))
    candidates.append(
        os.path.abspath(os.path.join(script_dir, "..", "..", "jobStarr-res", "JobSpy"))
    )

    # De-duplicate while preserving priority order
    seen = set()
    ordered = []
    for cand in candidates:
        if cand not in seen:
            seen.add(cand)
            ordered.append(cand)

    found_dir = None
    for cand in ordered:
        if os.path.isfile(os.path.join(cand, "jobspy", "__init__.py")):
            found_dir = cand
            break

    if not found_dir:
        sys.stderr.write(
            "Unable to locate the jobspy package.\n"
            "Expected the bundled copy at 'third_party/jobspy' relative to the "
            "jobStarr source tree or installation directory.\n"
            "Set JOBSTARR_JOBSPY_PATH to override, and install the runtime "
            "dependencies with: pip install -r tools/requirements-jobspy.txt\n"
        )
        sys.exit(1)

    if found_dir not in sys.path:
        sys.path.insert(0, found_dir)


discover_and_load_jobspy()

try:
    from jobspy.model import (
        Country,
        DescriptionFormat,
        JobPost,
        JobType,
        Location,
        ScraperInput,
        Site,
    )
    from jobspy.linkedin import LinkedIn
    from jobspy.linkedin.util import is_job_remote
    from jobspy.indeed import Indeed
    from jobspy.indeed.constant import api_headers, job_search_query
    from jobspy.util import extract_emails_from_text
    from bs4 import BeautifulSoup
except Exception as exc:
    sys.stderr.write(f"JobSpy import error: {exc}\n")
    sys.exit(1)


def is_valid_linkedin_host(host: str) -> bool:
    host = host.lower()
    return host == "linkedin.com" or host.endswith(".linkedin.com")


def is_valid_indeed_host(host: str) -> bool:
    host = host.lower()
    return host == "indeed.com" or host.endswith(".indeed.com") or ".indeed." in host


def extract_linkedin_job_id(url: str) -> Optional[str]:
    parsed = urlparse(url)
    # Check query param e.g. currentJobId=4464921447
    qs = parse_qs(parsed.query)
    if "currentJobId" in qs and qs["currentJobId"]:
        return qs["currentJobId"][0]

    # Check path e.g. /jobs/view/4464921447 or /jobs/view/slug-4464921447
    path_parts = [p for p in parsed.path.split("/") if p]
    for i, part in enumerate(path_parts):
        if part == "view" and i + 1 < len(path_parts):
            target = path_parts[i + 1]
            m = re.search(r"(\d{6,})", target)
            if m:
                return m.group(1)

    # Any 6+ digits at end of path
    m = re.search(r"(\d{6,})", parsed.path)
    if m:
        return m.group(1)

    return None


def extract_indeed_job_key(url: str) -> Optional[str]:
    parsed = urlparse(url)
    qs = parse_qs(parsed.query)
    for param in ("jk", "vjk", "jobkey"):
        if param in qs and qs[param]:
            return qs[param][0]

    # Pattern like /viewjob?jk=... or /rc/clk?jk=...
    m = re.search(r"[?&](?:jk|vjk)=([a-zA-Z0-9_-]+)", url)
    if m:
        return m.group(1)

    # Check path for /viewjob/...
    path_parts = [p for p in parsed.path.split("/") if p]
    if path_parts:
        last = path_parts[-1]
        m = re.search(r"^([a-zA-Z0-9]{16,})$", last)
        if m:
            return m.group(1)

    return None


def scrape_linkedin_job(job_id: str) -> dict[str, Any]:
    scraper = LinkedIn()
    scraper.scraper_input = ScraperInput(
        site_type=[Site.LINKEDIN],
        country=Country.USA,
        fetch_description=True,
        description_format=DescriptionFormat.MARKDOWN,
    )

    # Fetch detail page
    url = f"{scraper.base_url}/jobs/view/{job_id}"
    try:
        resp = scraper.session.get(url, timeout=20)
        resp.raise_for_status()
    except Exception as e:
        sys.stderr.write(f"Scrape failure fetching LinkedIn job {job_id}: {e}\n")
        sys.exit(2)

    soup = BeautifulSoup(resp.text, "html.parser")

    # Title
    title_tag = (
        soup.find("h1", class_=lambda c: c and "top-card-layout__title" in c)
        or soup.find("h2", class_=lambda c: c and "top-card-layout__title" in c)
        or soup.find("span", class_="sr-only")
    )
    title = title_tag.get_text(strip=True) if title_tag else "N/A"

    # Company
    comp_tag = (
        soup.find("a", class_=lambda c: c and "topcard__org-name-link" in c)
        or soup.find("h4", class_="base-search-card__subtitle")
    )
    company = ""
    company_url = ""
    if comp_tag:
        if comp_tag.name == "a":
            company = comp_tag.get_text(strip=True)
            if comp_tag.has_attr("href"):
                company_url = urlunparse(urlparse(comp_tag["href"])._replace(query=""))
        else:
            comp_a = comp_tag.find("a")
            if comp_a:
                company = comp_a.get_text(strip=True)
                if comp_a.has_attr("href"):
                    company_url = urlunparse(urlparse(comp_a["href"])._replace(query=""))
            else:
                company = comp_tag.get_text(strip=True)

    # Location
    loc_tag = (
        soup.find("span", class_=lambda c: c and "topcard__flavor--bullet" in c)
        or soup.find("span", class_="job-search-card__location")
    )
    loc_str = loc_tag.get_text(strip=True) if loc_tag else ""
    parts = [p.strip() for p in loc_str.split(",") if p.strip()]
    if len(parts) == 1 and loc_str:
        loc_obj = Location(city=loc_str, country=Country.from_string(scraper.country))
    elif len(parts) == 2:
        loc_obj = Location(city=parts[0], state=parts[1], country=Country.from_string(scraper.country))
    elif len(parts) >= 3:
        loc_obj = Location(city=parts[0], state=parts[1], country=parts[2])
    else:
        loc_obj = Location(country=Country.from_string(scraper.country))

    # Date posted
    date_posted: Optional[str] = None
    time_tag = soup.find("time")
    if time_tag and "datetime" in time_tag.attrs:
        date_posted = time_tag["datetime"]
    else:
        time_ago_tag = soup.find("span", class_=lambda c: c and "posted-time-ago__text" in c)
        time_ago_text = time_ago_tag.get_text(strip=True) if time_ago_tag else ""
        m = re.search(r"(\d+)\s+(day|week|month|year|hour|minute)s?\s+ago", time_ago_text)
        if m:
            val, unit = int(m.group(1)), m.group(2)
            today = date.today()
            if unit in ("hour", "minute"):
                date_posted = today.isoformat()
            elif unit == "day":
                date_posted = (today - timedelta(days=val)).isoformat()
            elif unit == "week":
                date_posted = (today - timedelta(weeks=val)).isoformat()
            elif unit == "month":
                date_posted = (today - timedelta(days=val * 30)).isoformat()
            elif unit == "year":
                date_posted = (today - timedelta(days=val * 365)).isoformat()

    # Detail fields via JobSpy LinkedIn._get_job_details
    job_details = scraper._get_job_details(job_id)
    description = job_details.get("description")
    emails = extract_emails_from_text(description) if description else []

    job_type_val = None
    if job_details.get("job_type"):
        job_type_list = job_details["job_type"]
        if isinstance(job_type_list, list) and job_type_list:
            job_type_val = [jt.value[0] if isinstance(jt, JobType) else str(jt) for jt in job_type_list]

    comp_dict = None
    if job_details.get("compensation"):
        c = job_details["compensation"]
        comp_dict = {
            "interval": c.interval.value if hasattr(c.interval, "value") else (str(c.interval) if c.interval else None),
            "min_amount": c.min_amount,
            "max_amount": c.max_amount,
            "currency": c.currency,
        }

    loc_dict = {
        "city": loc_obj.city,
        "state": loc_obj.state,
        "country": (loc_obj.country.value[0].split(",")[0].upper() if hasattr(loc_obj.country, "value")
                    else str(loc_obj.country) if loc_obj.country else None),
    }

    return {
        "source": "linkedin",
        "id": f"li-{job_id}",
        "title": title,
        "company_name": company or None,
        "company_url": company_url or None,
        "company_url_direct": None,
        "location": loc_dict,
        "date_posted": date_posted,
        "job_url": f"{scraper.base_url}/jobs/view/{job_id}",
        "job_url_direct": None,
        "is_remote": is_job_remote(title, loc_obj),
        "job_type": job_type_val,
        "job_level": job_details.get("job_level"),
        "job_function": job_details.get("job_function"),
        "listing_type": None,
        "compensation": comp_dict,
        "description": description,
        "emails": emails,
        "company_industry": job_details.get("company_industry"),
        "company_addresses": None,
        "company_num_employees": None,
        "company_revenue": None,
        "company_description": None,
        "company_logo": job_details.get("company_logo"),
    }


def scrape_indeed_job(job_key: str, original_url: str) -> dict[str, Any]:
    scraper = Indeed()

    # Determine country from domain
    parsed = urlparse(original_url)
    host = parsed.netloc.lower()
    country_enum = Country.USA
    for c in Country:
        try:
            subdomain, _ = c.indeed_domain_value
            if f"{subdomain}.indeed" in host:
                country_enum = c
                break
        except Exception:
            continue

    scraper.scraper_input = ScraperInput(
        site_type=[Site.INDEED],
        country=country_enum,
        description_format=DescriptionFormat.MARKDOWN,
    )
    domain, scraper.api_country_code = scraper.scraper_input.country.indeed_domain_value
    scraper.base_url = f"https://{domain}.indeed.com"

    h = api_headers.copy()
    h["indeed-co"] = scraper.api_country_code
    h["indeed-locale"] = "en-US"

    # Extract job field list from JobSpy's job_search_query
    raw = job_search_query.replace("{{", "{").replace("}}", "}")
    idx = raw.find("job {")
    end = raw.rfind("}")
    job_fields = raw[idx + 5 : raw.rfind("}", 0, raw.rfind("}", 0, raw.rfind("}", 0, end)))]

    query = f"""
    query {{
        jobData(jobKeys: ["{job_key}"]) {{
            results {{
                job {{
                    {job_fields}
                }}
            }}
        }}
    }}
    """

    try:
        resp = scraper.session.post(scraper.api_url, headers=h, json={"query": query}, timeout=20)
        resp.raise_for_status()
        data = resp.json()
    except Exception as e:
        sys.stderr.write(f"Scrape failure fetching Indeed job {job_key}: {e}\n")
        sys.exit(2)

    results = (data.get("data") or {}).get("jobData", {}).get("results", [])
    if not results or not results[0].get("job"):
        sys.stderr.write(f"Indeed job not found for key: {job_key}\n")
        sys.exit(3)

    job_dict = results[0]["job"]
    processed: Optional[JobPost] = scraper._process_job(job_dict)
    if not processed:
        sys.stderr.write(f"Failed to process Indeed job key: {job_key}\n")
        sys.exit(3)

    comp_dict = None
    if processed.compensation:
        c = processed.compensation
        comp_dict = {
            "interval": c.interval.value if hasattr(c.interval, "value") else (str(c.interval) if c.interval else None),
            "min_amount": c.min_amount,
            "max_amount": c.max_amount,
            "currency": c.currency,
        }

    loc_dict = {}
    if processed.location:
        loc_dict = {
            "city": processed.location.city,
            "state": processed.location.state,
            "country": (processed.location.country.value[0].split(",")[0].upper()
                        if hasattr(processed.location.country, "value")
                        else str(processed.location.country) if processed.location.country else None),
        }

    job_type_val = None
    if processed.job_type:
        job_type_val = [jt.value[0] if isinstance(jt, JobType) else str(jt) for jt in processed.job_type]

    return {
        "source": "indeed",
        "id": processed.id,
        "title": processed.title,
        "company_name": processed.company_name,
        "company_url": processed.company_url,
        "company_url_direct": processed.company_url_direct,
        "location": loc_dict,
        "date_posted": str(processed.date_posted) if processed.date_posted else None,
        "job_url": processed.job_url,
        "job_url_direct": processed.job_url_direct,
        "is_remote": processed.is_remote,
        "job_type": job_type_val,
        "job_level": None,
        "job_function": None,
        "listing_type": processed.listing_type,
        "compensation": comp_dict,
        "description": processed.description,
        "emails": processed.emails or [],
        "company_industry": processed.company_industry,
        "company_addresses": processed.company_addresses,
        "company_num_employees": processed.company_num_employees,
        "company_revenue": processed.company_revenue,
        "company_description": processed.company_description,
        "company_logo": processed.company_logo,
    }


def main():
    parser = argparse.ArgumentParser(description="JobSpy Bridge for jobStarr")
    parser.add_argument("--url", required=True, help="Job posting URL (LinkedIn or Indeed)")
    args = parser.parse_args()

    url = args.url.strip()
    parsed = urlparse(url)

    if not parsed.scheme or not parsed.netloc:
        sys.stderr.write(f"Invalid URL: {url}\n")
        sys.exit(1)

    host = parsed.netloc.lower()

    if is_valid_linkedin_host(host):
        job_id = extract_linkedin_job_id(url)
        if not job_id:
            sys.stderr.write(f"Unable to extract LinkedIn job ID from URL: {url}\n")
            sys.exit(1)
        record = scrape_linkedin_job(job_id)
    elif is_valid_indeed_host(host):
        job_key = extract_indeed_job_key(url)
        if not job_key:
            sys.stderr.write(f"Unable to extract Indeed job key from URL: {url}\n")
            sys.exit(1)
        record = scrape_indeed_job(job_key, url)
    else:
        sys.stderr.write(f"Unsupported job host: {host}. Only LinkedIn and Indeed are supported.\n")
        sys.exit(1)

    # Output ONLY valid pretty-printed JSON to stdout
    stdout_content = json.dumps(record, indent=2, ensure_ascii=False)
    sys.stdout.write(stdout_content + "\n")
    sys.stdout.flush()
    sys.exit(0)


if __name__ == "__main__":
    main()
