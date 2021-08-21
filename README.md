## About
This is a fork of the satellite tracking application [gpredict](https://github.com/csete/gpredict), adding iCalendar functionality. 

When viewing a pass or a set of passes you will see an extra button, which easily allows you to export said pass of set of passes as an .ics-File.

That way you can easily add a satellite pass to your calendar software 
of choice, which can be very useful for satellite operations planning.

Refer to the original [gpredict GitHub](https://github.com/csete/gpredict) for further information about the application itself.

## Getting started
Currently, you will have to build from source. The following libraries with their development packages are required:

- Gtk+ 3 or later
- GLib 2.32 or later
- GooCanvas 2
- Libcurl 7.16 or later
- Hamlib (runtime only, not required for build)


On Debian-based systems, you can install all the dependencies using the following commands:
```sh 
sudo apt install libtool intltool autoconf automake libcurl4-openssl-dev

sudo apt install pkg-config libglib2.0-dev libgtk-3-dev libgoocanvas-2.0-dev
```
### Installation
0. Make sure you have the dependencies listed above installed.


1. Clone this repo or download the source code as a zip and unpack it
```sh
git clone https://www.github.com/Apace33/gpredict-ical
```


2. Run the Autogen script
```sh
./autogen.sh
```


3. Build and install gpredict with the following commands
```sh
  make
  sudo make install
```
If you can not or do not want to
install gpredict as root, you can install gpredict into a custom directory by
adding `--prefix=somedir` step. For example
```sh
  ./autogen.sh --prefix=/home/user/predict
```
will configure the build to install the files into /home/user/gpredict folder.

## License
As gpredict is licensed under the GNU General Public License, this fork has to be as well. Refer to `COPYING` for further details.
