dolphin-perforce-plugin
=======================
Simple Perforce version control plugin for Dolphin inspiered by the version control plugins kdesdk-dolphin-pligins


Features
========
Displayes the state of the files inder Perforce control and adds a simple right-click menu ("open for edit", "add", "delete", "update", "diff", etc.)


Notes
=====
The plugin has runtime dependencies on P4, P4V, and P4VC version 2012.1 or later, you probaly already got this if you are using Perforce on the pc. However it can be found here: http://www.perforce.com/

The user needs to ensure that the environment variable P4CONFIG is set and that a coresponding file is placed in the top-level directory under Perforce control.

The user needs to ensure that P4DIFF is not set inside the P4CONFIG file

Perforce clients with "client root" pointing at a symlink will not work. The user must point the perforce "client root" to the canonical file path (it might also work to have the canonical file path configuret as "alternative root"). Sorry for the inconvienence, but UNIX symlinks are known to cause problems for Perforce see e.g. http://kb.perforce.com/UserTasks/ConfiguringP4/SymbolicLinks.

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
