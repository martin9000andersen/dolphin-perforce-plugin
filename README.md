dolphin-perforce-plugin
=======================
Simple Perforce version control plugin for Dolphin inspiered by the version control plugins kdesdk-dolphin-pligins


Features
========
Displayes the state of the files inder Perforce control and adds a simple right-click menu ("open for edit", "add", "delete", "update", "diff", etc.)


Notes
=====
The plugin has a runtime dependency on P4 (The Perforce Command-Line Client), you probaly already got this if you are using Perforce on the pc. However it can be found here: http://www.perforce.com/product/components/perforce_commandline_client

The user needs to ensure that the environment variable P4CONFIG is set and that a coresponding file is placed in the toplevel directory under Perforce control.

The user needs to ensure that the environment variable P4CONFIG is set and that a coresponding file is placed in the top-level directory under Perforce control.

The user needs to ensure that P4DIFF is not set inside the P4CONFIG file


Installation
============
First install the build dependencies. On (K)Ubuntu the following command should install everything you need:
	sudo apt-get build-dep dolphin

And the installation:
	mkdir -p build
	cd build
	cmake .. -DCMAKE_INSTALL_PREFIX=`kde4-config --prefix`
	make
	sudo make install

Install dependencies
	Kompare (www.kde.org/applications/development/kompare/)
		sudo apt-get install kompare


Activate the Plugin
===================
Restart Dolphin
In the menu
	Settings -> Configure Dolphin... -> Services
Select Perforce on the list
