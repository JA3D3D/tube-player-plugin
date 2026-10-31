# AbletonAi Live helper (control surface script for Live 11 and newer).
#
# Install: this folder goes in  ~/Music/Ableton/User Library/Remote Scripts/AbletonAi
# Switch on: Live > Settings > Link, Tempo & MIDI > Control Surface: AbletonAi (Input/Output: None)
#
# It lets the AbletonAi plugin build in your set: tracks, instruments and effects from Live's
# browser, MIDI clips, device settings, tempo. It only listens on your own computer.

from __future__ import absolute_import, print_function, unicode_literals

import Live
from _Framework.ControlSurface import ControlSurface

from .core import Commands, VERSION
from .server import Server


def create_instance(c_instance):
    return AbletonAiHelper(c_instance)


class AbletonAiHelper(ControlSurface):

    def __init__(self, c_instance):
        ControlSurface.__init__(self, c_instance)
        self._commands = Commands(self.song, self.application, Live, self.log_message)
        self._server = Server(self._commands, log=self.log_message)
        if self._server.start():
            self.show_message("AbletonAi helper {0} is on".format(VERSION))
        else:
            self.show_message("AbletonAi helper couldn't start (no free port)")

    def update_display(self):
        ControlSurface.update_display(self)
        try:
            self._server.process()
        except Exception as e:  # never let a bad request take the control surface down
            self.log_message("AbletonAi: " + str(e))

    def disconnect(self):
        try:
            self._server.stop()
        finally:
            ControlSurface.disconnect(self)
