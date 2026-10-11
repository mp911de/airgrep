"""Which carrier project the tools and simulations read.

Two KiCad projects live under hardware/: carrier-max-charger (MAX77757) and carrier-ti-charger
(BQ25798). Scripts take --project NAME, read CARRIER_PROJECT from the environment, or default
to carrier-ti-charger. Works with KiCad's Python 3.9 and the sim venv.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = "carrier-ti-charger"
# project directory name -> charger tag used for sim constants and output folders
CHARGERS = {"carrier-max-charger": "max", "carrier-ti-charger": "ti"}


def resolve(name=None):
    """Absolute project directory for a name, a path, the environment or the default."""
    name = name or os.environ.get("CARRIER_PROJECT") or DEFAULT
    path = name if os.path.isdir(name) else os.path.join(ROOT, "hardware", name)
    if not os.path.isfile(os.path.join(path, "carrier.kicad_pcb")):
        raise SystemExit("no carrier.kicad_pcb in %s (projects: %s)" % (path, ", ".join(sorted(CHARGERS))))
    return os.path.abspath(path)


def charger(project):
    """Charger tag of a project directory."""
    try:
        return CHARGERS[os.path.basename(os.path.normpath(project))]
    except KeyError:
        raise SystemExit("unknown project %s, expected one of %s" % (project, ", ".join(sorted(CHARGERS))))


def pcb(project):
    return os.path.join(project, "carrier.kicad_pcb")


def sch(project):
    return os.path.join(project, "carrier.kicad_sch")


def add_argument(parser):
    parser.add_argument("--project", metavar="NAME", default=None,
                        help="carrier project under hardware/ (default $CARRIER_PROJECT or %s)" % DEFAULT)


def select(name):
    """Make a --project choice visible to every module that reads the environment."""
    path = resolve(name)
    os.environ["CARRIER_PROJECT"] = path
    return path
