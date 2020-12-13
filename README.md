Program to capture data from some LivingSense-compatible weather stations
=========================================================================

A program to receive and decode network traffic from some WiFi-enabled weather
stations - the many rebranded versions of EMAX EM3371:
        - Meteo SP73,
        - GreenBlue GB522,
        - Meteo Logic EM3371,
        - DIGOO DG-EX001.

This program has been tested on a Meteo SP73 weather station, which was bought
in late 2020. It is based on reverse-engineering work performed by Mr Łukasz
Kalamłacki: http://kalamlacki.eu/sp73.php

Notice: this is a third party program. It is neither endorsed nor supported by
EMAX or the above weather station brands. All trademarks are used only for
the purpose of showing compatibility or potential compatibility.

        pon, 2 lis 2020, 18:07:07 CET
        - weather station Meteo Logic EM3371 looks similar, so it
        may also be compatible:
        https://www.youtube.com/watch?v=G7D54pm1FjE

        DIGOO DG-EX001 should also be the same model rebranded,

        EDIT: pon, 2 lis 2020, 18:59:16 CET
        http://www.emaxtime.com/product/13.html
                - product info on the manufacturer's website, where the station
                  is called "EM3371 Wifi weather [sic]"

Supported operating systems
---------------------------

This program was written in a way that enabled it to be used on constrained
resource devices, such as home routers.

This program runs on GNU/Linux and DD-WRT. It was not tested on OpenWRT, but

Program do przetwarzania danych wysyłanych przez stację pogody Meteo SP73.
Ta stacja pogody jest prawie identyczna z wyglądu do stacji pogody GreenBlue
GB522.
Stacje Meteo SP73 oraz GreenBlue GB522 wyglądają identycznie, poza logotypami
marki.

Program działa pod kontrolą GNU/Linuksa, w tym także DD-WRT. Powinien zadziałać
bez problemu na Raspberry Pi. Nie był testowany na OpenWRT, jednak dostosowanie
go do tego środowiska powinno być proste.

Przeniesienie na Windowsa (przy użyciu MinGW) wymagałoby pewnej ilości pracy,
jednak z użyciem biblioteki Gnulib też mogłoby być stosunkowo proste.

Program jest oparty na pracy wykonanej przez Pana Łukasza Kalamłackiego, który
wykonał analizę wsteczną protokołu stacji:
http://kalamlacki.eu/sp73.php

Raporty z używania programu mile widziane - przez bug report na GitHubie lub
mail na mat.jonczyk w domenie o2.pl.

## Opis konfiguracji stacji


Formaty wyjścia programu (JSON, CSV) mogą się zmienić w przyszłej wersji.


To configure the weather station:

1. Press the "WIFI / UP" button for three seconds. The "AP" text will be shown
   on the weather station display (in the place where day of week is shown) and
   it will start flashing.
2. Connect to the WiFi network called "LivingSmart".
3. Open in a web browser the address http://11.11.11.254/. Be patient - the
   WebGUI is *very* slow and can take up to several minutes to load.
4. Login using "admin" / "admin" credentials. The menu is all in Chinese, but
   the identifiers in the HTML code are in English. An online translator may
   also help.
5. If the device is not already connected to the WiFi network, select "STA设置"
   in the menu on the left hand side and set WiFi settings.
6. In the menu on the left hand side, select "其他设置". After a while a new
   window will load, wait till is loaded fully and some values will show in the
   form. In the bottom there is an option to specify server ("服务器地址",
   default value "SMARTSERVER.EMAXTIME.CN") and port number ("端口", default
   value 17000) to which the device will send sensor data.
7. After submitting the form, choose "重启" ("restart, reboot").
   It seems that it is the WiFi module that is restarted, not the whole weather
   station.
8. Again press the "WIFI/UP" button for three seconds - until the "AP" text will
   be replaced with the day of week.

When the weather station WiFi module is running in the Access Point mode, the
WebGUI is very sluggish, but in STA mode (when connected to a WiFi network as a
client device) it responds much faster.

Probably the device does not send measurement packets in Access Point mode.



## General remarks on weather station use

A good guide on weather station sensor placement can be found at
        https://www.wunderground.com/pws/installation-guide

### How to check weather station sensor reliability

This procedure requires that the outdoor temperature is lower then the indoor
temperature.

1. Place one sensor inside the room and one outside.
2. Air the room vigoriously for a while - long enough that most air is
   exchanged and it feels cold inside the room, but not too long - so that the
   walls and furniture do not get cold.
3. Close the windows, exit the room and close the door behind. Wait a while ---
   the air should quickly get warmer and sensor readings stabilize.
4. Compare dew point inside and outside - as calculated from measurements
   provided by both sensors. Sensors inside and outside will provide different
   humidity and temperature readings, but dew point values should be similar.

Air (especially dry air) has low heat capacity so after closing the windows it
will quickly get warmer from walls and furniture.



and wait a while so that


## Compilation for DD-WRT

It is necessary to use a toolchain that links code with the C library used in the DD-WRT build,
which frequently is uClibc. Cross compilers supplied with Linux distributions
usually generate code for GNU libc and so the program compiled with them may
not work.

Compilers for DD-WRT can be obtained from
        https://dd-wrt.com/support/other-downloads/
path `toolchains/toolchains.tar.xz`. It is a large download, but only a part of it will be needed
--- it is not necessary to decompress the whole file.

It is not straightforward to determine which toolchain to use and also required command line
parameters of GCC. Some hints:

1. Login via SSH and check `/proc/cpuinfo` and top of `dmesg`. This will tell the architecture and
   version of the compiler that was used to build the kernel.
2. Look at the DD-WRT source code and build scripts on https://github.com/mirror/dd-wrt/ .
   The source code is unfortunately messy and it is not easy to determine which build scripts are
   really used and which are just leftovers sitting idly in the repository.
   Proper build scripts seem to be in `src/router` (GCC command line parameters may be found in this
   file), toolchain used may be defined in `src/router/configs/buildscripts` (but files there may be
   out-of-date).

Some devices have no hardware floating point support. If so, it may be necessary to use
`-msoft-float` GCC command line parameter. Otherwise, the program may silently hang with possibly
the following message in dmesg:

    FPU emulator disabled, make sure your toolchainwas compiled with software floating point support (soft-float)

This problem occured with `toolchain-mipsel_gcc4.1.2` even with this command line parameter.
I therefore used `toolchain-mipsel_gcc-linaro_uClibc-0.9.32`, which works correctly.


## Usage on DD-WRT

- zalecane jest co najmniej 1MB wolnego miejsca na partycji /jffs,
  - partycja /jffs chyba nie jest domyślnie włączona, trzeba załączyć ją
    w panelu WEB GUI rutera i przed pierwszym użyciem sformatować,

  - jeśli nie ma takiej opcji, to można użyć mniejszych buildów DD-WRT,

- część danych można jednak trzymać w pamięci RAM, np. dzienniki debugowania
  (stderr programu),

- najlepiej jest użyć logrotate do rotacji i kompresji logów,
  - z załączoną opcją sharedscripts,

- plik CSV z jednego dnia nie skompresowany może zająć nawet 500kB, jeśli są obecne
  wszystkie 3 czujniki,

## TODO

- implement log file saving and rotation (useful especially on devices with
  limited storage space),
    - using `logrotate` could be easier and better, however,
- handle '--help' command line parameter,
- write proper README.md,
- reverse engineering:
    - devise a more accurate formula to calculate temperature from raw data,
      - it appears that the temperature in °F is equal to (raw_data/10)-90
    - describe how `historical1` and `historical2` in `device_single_sensor_data` behave,
    - decode other bytes in packets sent by the device,
    Some versions of the weather station are advertised to contain a DCF77 time
    signal receiver (from a transmitter near Frankfurt, Germany).
    Time received this way would be more reliable.
- use a Makefile instead of compile.sh
- Windows port
    - probably the best approach would be to use Gnulib:
            https://www.gnu.org/software/gnulib/
- maybe some refactoring,
- write text documentation of the protocol used by the weather station,
- a webpage that displays the last and historical measurements.
  It could be made using a single HTML file, with some JavaScript code that
  would download the results in JSON. The C program could provide a single text file
  that would contain only the last measurement in JSON.
  - but writing a WeeWX plugin would mostly cover that use case,
- sending data to a site like wunderground.com,
- integration into other projects that handle weather station data (I
  could search them on GitHub),
  - I did some research and writing a WeeWX plugin seems sensible,
  - or perhaps modifying wview, but it seems not to be maintained any longer,

- find other weather stations that should be compatible with this software,
  - searching for "Livingsense" (the mobile app name) may be helpful,
- dew point calculation: check which parameters to choose when the dry bulb
  temperature is above 0°C, but the dew point / frost point is below 0°C,
- change license to GPLv2+, as the MySQL libraries are not compatible with GPLv3.
