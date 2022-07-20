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

extensions = ["sphinxcontrib.fulltoc", "breathe"]
