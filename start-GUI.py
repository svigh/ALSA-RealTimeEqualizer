from gi.repository import Gtk
import subprocess as sp
import os

CWD = os.path.dirname(os.path.realpath(__file__))
AUDIO_START_CMD = "./playback"
AUDIO_COMPILE_CMD = "gcc -Wall -o playback playback.c utils.c effects.c -lasound -w -lpthread -lfftw3 -lm"
NUM_EQ_BANDS = 12
EQ_VALS_FILE = "eq_vals.txt"

EQ_bands = [1 for x in range(NUM_EQ_BANDS)]
EQ_format = "%s %s %s %s %s %s %s %s %s %s %s %s"


def write_eq_vals_to_file():
	with open(EQ_VALS_FILE, "w") as eq_vals:
		eq_vals.write( EQ_format % (str(EQ_bands[0]), str(EQ_bands[1]), str(EQ_bands[2]),\
			str(EQ_bands[3]), str(EQ_bands[4]), str(EQ_bands[5]), str(EQ_bands[6]),\
			str(EQ_bands[7]), str(EQ_bands[8]), str(EQ_bands[9]), str(EQ_bands[10]), str(EQ_bands[11])))


try:
	sp_audio = sp.Popen(AUDIO_START_CMD, stdin=sp.PIPE, stdout=sp.PIPE, cwd=CWD)
except:
	print("Tried starting program, it doesn't exist, trying to recompile.")
	os.system(AUDIO_COMPILE_CMD)
	sp_audio = sp.Popen(AUDIO_START_CMD, stdin=sp.PIPE, stdout=sp.PIPE, cwd=CWD)



# TODO: rework duplicated code for the scales value changes
class Handler:
	global EQ_bands

	def StopEffectsButton_clicked(self, button):
		print("Remove all effects")
		sp_audio.stdin.write(b"1")
		sp_audio.stdin.flush()

	def AddEchoButton_clicked(self, button):
		print("Add echo")
		sp_audio.stdin.write(b"2")
		sp_audio.stdin.flush()

	def AddGainButton_clicked(self, button):
		print("Add gain")
		sp_audio.stdin.write(b"3")
		sp_audio.stdin.flush()

	def AddDistortionButton_clicked(self, button):
		print("Add distortion")
		sp_audio.stdin.write(b"4")
		sp_audio.stdin.flush()

	def AddEqualizerButton_clicked(self, button):
		# To initialize the EQ vals with what is currently selected on the scales
		write_eq_vals_to_file()

		print("Add Equalizer")
		sp_audio.stdin.write(b"5")
		sp_audio.stdin.flush()


	def Scale_0_value_changed(self, scale):
		EQ_bands[0] = Scale_0.get_value()
		write_eq_vals_to_file()

	def Scale_1_value_changed(self, scale):
		EQ_bands[1] = Scale_1.get_value()
		write_eq_vals_to_file()

	def Scale_2_value_changed(self, scale):
		EQ_bands[2] = Scale_2.get_value()
		write_eq_vals_to_file()

	def Scale_3_value_changed(self, scale):
		EQ_bands[3] = Scale_3.get_value()
		write_eq_vals_to_file()

	def Scale_4_value_changed(self, scale):
		EQ_bands[4] = Scale_4.get_value()
		write_eq_vals_to_file()

	def Scale_5_value_changed(self, scale):
		EQ_bands[5] = Scale_5.get_value()
		write_eq_vals_to_file()

	def Scale_6_value_changed(self, scale):
		EQ_bands[6] = Scale_6.get_value()
		write_eq_vals_to_file()

	def Scale_7_value_changed(self, scale):
		EQ_bands[7] = Scale_7.get_value()
		write_eq_vals_to_file()

	def Scale_8_value_changed(self, scale):
		EQ_bands[8] = Scale_8.get_value()
		write_eq_vals_to_file()

	def Scale_9_value_changed(self, scale):
		EQ_bands[9] = Scale_9.get_value()
		write_eq_vals_to_file()


builder = Gtk.Builder()
builder.add_from_file("user-interface.glade")
builder.connect_signals(Handler())
main_window = builder.get_object("MainWindow")

Notebook = builder.get_object("Notebook")

# Functional buttons
# StopEffectsButton = builder.get_object("StopEffectsButton")
# AddEchoButton = builder.get_object("AddEchoButton")
Scale_0 = builder.get_object("adjustment0")
Scale_1 = builder.get_object("adjustment1")
Scale_2 = builder.get_object("adjustment2")
Scale_3 = builder.get_object("adjustment3")
Scale_4 = builder.get_object("adjustment4")
Scale_5 = builder.get_object("adjustment5")
Scale_6 = builder.get_object("adjustment6")
Scale_7 = builder.get_object("adjustment7")
Scale_8 = builder.get_object("adjustment8")
Scale_9 = builder.get_object("adjustment9")


main_window.connect("delete-event", Gtk.main_quit)
main_window.fullscreen()
main_window.show_all()

Gtk.main()
