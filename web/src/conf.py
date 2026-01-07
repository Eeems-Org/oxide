import datetime

year = datetime.datetime.today().year

project = "Oxide"
copyright = f"2021-{year}, Eeems"
author = "Nathaniel 'Eeems' van Diepen"

html_theme_path = ["_themes"]
html_static_path = ["_static"]
templates_path = ["_templates"]

html_title = "Oxide"
html_theme = "oxide"
master_doc = "sitemap"
html_sidebars = {"**": ["nav.html", "sidefooter.html"]}
html_permalinks_icon = "#"
html_css_files = ["oxide.css"]
html_js_files = [
    "https://peek.eeems.website/peek.js",
    "oxide.js",
    (
        "https://browser.sentry-cdn.com/10.20.0/bundle.tracing.min.js",
        {
            "integrity": "sha384-+9wUXHfFUeGwAnEesSxSFwKwBHDbnc32jfpX3HN0aB9RHa4lbS1rqsSnM6DI9/LM",
            "crossorigin": "anonymous",
        },
    ),
]

extensions = ["sphinxcontrib.fulltoc", "breathe"]
