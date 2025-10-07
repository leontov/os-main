from __future__ import annotations

import os
import sys
from datetime import datetime

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

project = "Kolibri SDK"
author = "Kolibri AI"
release = "0.5.0"
year = datetime.now().year

extensions = [
    "myst_parser",
    "sphinx.ext.autodoc",
    "sphinx.ext.autosectionlabel",
]

html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]
html_title = project

myst_enable_extensions = ["colon_fence", "deflist"]

autodoc_typehints = "description"
