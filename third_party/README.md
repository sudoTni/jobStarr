# Third-Party Code

This directory contains code vendored from an upstream project so that `jobStarr`
is self-contained and does not require a sibling checkout at runtime.

---

## jobspy/

| Field | Value |
| :--- | :--- |
| Package | [`jobspy`](https://github.com/speedyapply/JobSpy) |
| Upstream | https://github.com/speedyapply/JobSpy |
| Distribution | `python-jobspy` on PyPI |
| Version | **1.2.0** |
| Upstream commit | `83efd3d2ee7fce72d2b4cc4adb670f7e2b0e0e6b` — *chore: prepare 1.2.0 (#401)* (2026-10-02) |
| Authors | Cullen Watson, Zachary Hampton; maintained by Kirk Watson |
| License | MIT — see [`LICENSE.JobSpy`](LICENSE.JobSpy) |
| Requires Python | >= 3.10 |

### Why it is vendored

`tools/jobspy_bridge.py` imports `jobspy` at runtime. That import previously
required a separate `jobStarr-res/JobSpy` checkout at a fixed relative path,
which made `jobStarr` fail to scrape anything unless the user happened to have
sibling repositories in the expected layout. The package is now bundled here and
copied next to the compiled executable by the CMake `POST_BUILD` step, so a
fresh clone builds and runs without any sibling checkout.

Note that `jobspy/__init__.py` imports **all** site scrapers and `pandas`, so the
whole package is required even though `jobStarr` only exercises LinkedIn and
Indeed. It is vendored in full rather than pruned, to keep the copy identical to
upstream and therefore easy to re-sync.

### Runtime dependencies (not vendored)

These are PyPI packages and are **not** bundled — install them with:

```bash
pip install -r tools/requirements-jobspy.txt
```

| Package | Minimum (from upstream `pyproject.toml`) | Used for |
| :--- | :--- | :--- |
| `requests` | 2.31.0 | HTTP transport |
| `beautifulsoup4` | 4.12.2 | HTML parsing |
| `pandas` | 2.1.2 | required by `jobspy/__init__.py` |
| `pydantic` | 2.3.0 | `jobspy.model` schemas |
| `curl_cffi` | 0.16.2 | browser-impersonating TLS client |
| `markdownify` | 1.1.0 | HTML → Markdown job descriptions |

### Overrides

If you want to run against a different JobSpy build, set `JOBSTARR_JOBSPY_PATH` to
the directory *containing* the `jobspy/` package. It takes precedence over the
bundled copy.

### Updating

1. Update the upstream checkout and note its version and commit.
2. Replace the package contents, keeping the directory layout:
   ```bash
   rsync -a --delete --exclude='__pycache__' --exclude='*.pyc' \
         ../jobStarr-res/JobSpy/jobspy/ third_party/jobspy/
   ```
3. Refresh `LICENSE.JobSpy` if upstream's licence text changed.
4. Update the version/commit table above.
5. Reconcile `tools/requirements-jobspy.txt` against upstream's `pyproject.toml`.
6. Verify the import still succeeds:
   ```bash
   python3 tools/jobspy_bridge.py --url https://www.linkedin.com/jobs/view/4464921447
   ```
   (Requires network access and the runtime dependencies. A network-free smoke
   test of discovery alone is:
   `python3 -c "import sys; sys.argv=['x']; exec(open('tools/jobspy_bridge.py').read().split('def scrape_linkedin_job')[0]); print('ok')"`)

Do **not** vendor `__pycache__/`, `*.pyc`, or the upstream `.git` directory.