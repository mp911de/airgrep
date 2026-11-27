"""dmgbuild settings for the macOS airgrep Utility disk image."""

defines = globals()["defines"]

format = "ULFO"
files = [defines["app"]]
symlinks = {"Applications": "/Applications"}

background = defines["background"]
icon = defines["icon"]

# Outer window bounds: 600x400 canvas plus 28 pt title bar.
window_rect = ((200, 120), (600, 428))
default_view = "icon-view"
icon_size = 104
text_size = 13
icon_locations = {
    "airgrep Utility.app": (170, 215),
    "Applications": (430, 215),
}

show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
