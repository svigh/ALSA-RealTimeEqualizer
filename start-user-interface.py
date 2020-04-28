from gi.repository import Gtk
import subprocess as sp

AUDIO_START_CMD = "./playback"

sp_audio = sp.Popen(AUDIO_START_CMD, stdin=sp.PIPE, stdout=sp.PIPE)

class Handler:
	def StopEffectsButton_clicked(self, button):
		print("Remove all effects")
		sp_audio.stdin.write(b"2")
		sp_audio.stdin.flush()

	def AddEchoButton_clicked(self, button):
		print("Add echo")
		sp_audio.stdin.write(b"1")
		sp_audio.stdin.flush()

builder = Gtk.Builder()
builder.add_from_file("user-interface.glade")
builder.connect_signals(Handler())

# Not needed right now, might need in the future
StopEffectsButton = builder.get_object("StopEffectsButton")
AddEchoButton = builder.get_object("AddEchoButton")

window = builder.get_object("MainWindow")

window.connect("delete-event", Gtk.main_quit)
window.show_all()
Gtk.main()