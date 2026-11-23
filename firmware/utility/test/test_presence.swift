// What becomes of the page of a conversation while its device leaves the bus and comes back.

func testPresence() {
    var unplugged = Presence()
    check(!unplugged.isOver(ended: false, asked: false, listed: true, held: false), "a conversation that runs keeps its page")
    check(!unplugged.isOver(ended: false, asked: false, listed: false, held: false), "also while a listing misses a device that still answers")
    check(!unplugged.isOver(ended: true, asked: false, listed: true, held: false), "one that ended with its device on the bus stays readable")
    check(unplugged.isOver(ended: true, asked: false, listed: false, held: false), "one whose device left is over")

    // An install: the device enters its bootloader, is written, restarts, and the sheet is closed.
    var installed = Presence()
    check(!installed.isOver(ended: true, asked: false, listed: true, held: true), "the device is on its way into the bootloader")
    check(!installed.isOver(ended: true, asked: false, listed: false, held: true), "the sheet keeps the page while the device is in its bootloader")
    check(!installed.isOver(ended: true, asked: false, listed: true, held: true), "and while it reads Installed with the device back")
    check(installed.isOver(ended: true, asked: false, listed: true, held: false), "the page of the old conversation goes once the sheet is closed")

    // A restart: the device is back before a listing could miss it.
    var restarted = Presence()
    check(!restarted.isOver(ended: false, asked: true, listed: true, held: false), "a restart that is on its way keeps the page")
    check(restarted.isOver(ended: true, asked: true, listed: true, held: false), "a conversation the user ended is over at once")
    var watched = Presence()
    check(!watched.isOver(ended: true, asked: true, listed: true, held: true), "unless a window still shows the device")

    var failed = Presence()
    _ = failed.isOver(ended: true, asked: false, listed: false, held: true)
    check(failed.isOver(ended: true, asked: false, listed: false, held: false), "and so it does for a device that stays in its bootloader")
}
