# libpostal: international street address NLP

[![Build Status](https://travis-ci.org/openvenues/libpostal.svg?branch=master)](https://travis-ci.org/openvenues/libpostal)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/openvenues/libpostal?branch=master&svg=true)](https://ci.appveyor.com/project/albarrentine/libpostal/branch/master)
[![License](https://img.shields.io/github/license/openvenues/libpostal.svg)](https://github.com/openvenues/libpostal/blob/master/LICENSE)
[![OpenCollective Sponsors](https://opencollective.com/libpostal/sponsors/badge.svg)](#sponsors)
[![OpenCollective Backers](https://opencollective.com/libpostal/backers/badge.svg)](#backers)

libpostal is a C library for parsing/normalizing street addresses around the world using statistical NLP and open data. The goal of this project is to understand location-based strings in every language, everywhere. For a more comprehensive overview of the research behind libpostal, be sure to check out the (lengthy) introductory blog posts:

- **Original post**: [Statistical NLP on OpenStreetMap](https://medium.com/@albarrentine/statistical-nlp-on-openstreetmap-b9d573e6cc86)
- **Follow-up for 1.0 release**: [Statistical NLP on OpenStreetMap: Part 2](https://medium.com/@albarrentine/statistical-nlp-on-openstreetmap-part-2-80405b988718)

<span>&#x1f1e7;&#x1f1f7;</span> <span>&#x1f1eb;&#x1f1ee;</span>  <span>&#x1f1f3;&#x1f1ec;</span> :jp: <span>&#x1f1fd;&#x1f1f0; </span> <span>&#x1f1e7;&#x1f1e9; </span> <span>&#x1f1f5;&#x1f1f1; </span> <span>&#x1f1fb;&#x1f1f3; </span> <span>&#x1f1e7;&#x1f1ea; </span> <span>&#x1f1f2;&#x1f1e6; </span> <span>&#x1f1fa;&#x1f1e6; </span> <span>&#x1f1ef;&#x1f1f2; </span> :ru: <span>&#x1f1ee;&#x1f1f3; </span> <span>&#x1f1f1;&#x1f1fb; </span> <span>&#x1f1e7;&#x1f1f4; </span> :de: <span>&#x1f1f8;&#x1f1f3; </span>  <span>&#x1f1e6;&#x1f1f2; </span> :kr: <span>&#x1f1f3;&#x1f1f4; </span>  <span>&#x1f1f2;&#x1f1fd; </span> <span>&#x1f1e8;&#x1f1ff; </span> <span>&#x1f1f9;&#x1f1f7; </span> :es: <span>&#x1f1f8;&#x1f1f8; </span> <span>&#x1f1ea;&#x1f1ea; </span> <span>&#x1f1e7;&#x1f1ed; </span> <span>&#x1f1f3;&#x1f1f1; </span> :cn:  <span>&#x1f1f5;&#x1f1f9; </span> <span>&#x1f1f5;&#x1f1f7; </span> :gb: <span>&#x1f1f5;&#x1f1f8; </span> 

Addresses and the locations they represent are essential for any application dealing with maps (place search, transportation, on-demand/delivery services, check-ins, reviews). Yet even the simplest addresses are packed with local conventions, abbreviations and context, making them difficult to index/query effectively with traditional full-text search engines. This library helps convert the free-form addresses that humans use into clean normalized forms suitable for machine comparison and full-text indexing. Though libpostal is not itself a full geocoder, it can be used as a preprocessing step to make any geocoding application smarter, simpler, and more consistent internationally.

<span>&#x1f1f7;&#x1f1f4; </span> <span>&#x1f1ec;&#x1f1ed; </span> <span>&#x1f1e6;&#x1f1fa; </span> <span>&#x1f1f2;&#x1f1fe; </span> <span>&#x1f1ed;&#x1f1f7; </span> <span>&#x1f1ed;&#x1f1f9; </span> :us: <span>&#x1f1ff;&#x1f1e6; </span> <span>&#x1f1f7;&#x1f1f8; </span> <span>&#x1f1e8;&#x1f1f1; </span> :it: <span>&#x1f1f0;&#x1f1ea; <span>&#x1f1e8;&#x1f1ed; </span> <span>&#x1f1e8;&#x1f1fa; </span> <span>&#x1f1f8;&#x1f1f0; </span> <span>&#x1f1e6;&#x1f1f4; </span> <span>&#x1f1e9;&#x1f1f0; </span> <span>&#x1f1f9;&#x1f1ff; </span> <span>&#x1f1e6;&#x1f1f1; </span> <span>&#x1f1e8;&#x1f1f4; </span> <span>&#x1f1ee;&#x1f1f1; </span> <span>&#x1f1ec;&#x1f1f9; </span>  :fr: <span>&#x1f1f5;&#x1f1ed; </span> <span>&#x1f1e6;&#x1f1f9; </span> <span>&#x1f1f1;&#x1f1e8; </span>  <span>&#x1f1ee;&#x1f1f8; <span>&#x1f1ee;&#x1f1e9; </span> </span> <span>&#x1f1e6;&#x1f1ea; </span> </span> <span>&#x1f1f8;&#x1f1f0; </span> <span>&#x1f1f9;&#x1f1f3; </span> <span>&#x1f1f0;&#x1f1ed; </span> <span>&#x1f1e6;&#x1f1f7; </span> <span>&#x1f1ed;&#x1f1f0; </span>

The core library is written in pure C. Language bindings for [Python](https://github.com/openvenues/pypostal), [Ruby](https://github.com/openvenues/ruby_postal), [Go](https://github.com/openvenues/gopostal), [Java](https://github.com/openvenues/jpostal), [PHP](https://github.com/openvenues/php-postal), and [NodeJS](https://github.com/openvenues/node-postal) are officially supported and it's easy to write bindings in other languages.

Sponsors
--------

If your company is using libpostal, consider asking your organization to sponsor the project. Interpreting what humans mean when they refer to locations is far from a solved problem, and sponsorships help us pursue new frontiers in geospatial NLP. As a sponsor, your company logo will appear prominently on the Github repo page along with a link to your site. [Sponsorship info](https://opencollective.com/libpostal#sponsor)

<a href="https://opencollective.com/libpostal/sponsor/0/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/0/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/1/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/1/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/2/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/2/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/3/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/3/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/4/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/4/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/5/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/5/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/6/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/6/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/7/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/7/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/8/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/8/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/9/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/9/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/10/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/10/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/11/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/11/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/12/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/12/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/13/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/13/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/14/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/14/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/15/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/15/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/16/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/16/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/17/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/17/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/18/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/18/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/19/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/19/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/20/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/20/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/21/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/21/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/22/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/22/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/23/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/23/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/24/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/24/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/25/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/25/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/26/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/26/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/27/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/27/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/28/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/28/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/sponsor/29/website" target="_blank"><img src="https://opencollective.com/libpostal/sponsor/29/avatar.svg"></a>

Backers
------------

Individual users can also help support open geo NLP research by making a monthly donation:

<a href="https://opencollective.com/libpostal/backer/0/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/0/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/1/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/1/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/2/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/2/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/3/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/3/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/4/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/4/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/5/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/5/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/6/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/6/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/7/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/7/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/8/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/8/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/9/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/9/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/10/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/10/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/11/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/11/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/12/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/12/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/13/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/13/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/14/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/14/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/15/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/15/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/16/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/16/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/17/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/17/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/18/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/18/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/19/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/19/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/20/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/20/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/21/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/21/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/22/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/22/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/23/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/23/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/24/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/24/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/25/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/25/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/26/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/26/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/27/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/27/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/28/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/28/avatar.svg"></a>
<a href="https://opencollective.com/libpostal/backer/29/website" target="_blank"><img src="https://opencollective.com/libpostal/backer/29/avatar.svg"></a>

Installation (Mac/Linux)
------------------------

Before you install, make sure you have the following prerequisites:

**On Ubuntu/Debian**
```
sudo apt-get install curl autoconf automake libtool pkg-config
```

**On CentOS/RHEL**
```
sudo yum install curl autoconf automake libtool pkgconfig
```

**On Mac OSX**
```
brew install curl autoconf automake libtool pkg-config
```

Then to install the C library:

```
git clone https://github.com/openvenues/libpostal
cd libpostal
./bootstrap.sh
./configure --datadir=[...some dir with a few GB of space...]
make -j4
sudo make install

# On Linux it's probably a good idea to run
sudo ldconfig
```

libpostal has support for pkg-config, so you can use the pkg-config to print the flags needed to link your program against it:

```
pkg-config --cflags libpostal         # print compiler flags
pkg-config --libs libpostal           # print linker flags
pkg-config --cflags --libs libpostal  # print both
```

For example, if you write a program called app.c, you can compile it like this:

```
gcc app.c `pkg-config --cflags --libs libpostal`
```

Installation (Windows)
----------------------

**MSys2/MinGW**

For Windows the build procedure currently requires MSys2 and MinGW. This can be downloaded from http://msys2.org. Please follow the instructions on the MSys2 website for installation.

Please ensure Msys2 is up-to-date by running:
```
pacman -Syu
```

Install the following prerequisites:
```
pacman -S autoconf automake curl git make libtool gcc mingw-w64-x86_64-gcc
```

Then to build the C library:
```
git clone https://github.com/openvenues/libpostal
cd libpostal
cp -rf windows/* ./
./bootstrap.sh
./configure --datadir=[...some dir with a few GB of space...]
make -j4
make install
```
Notes: When setting the datadir, the `C:` drive would be entered as `/c`. The libpostal build script automatically add `libpostal` on the end of the path, so '/c' would become `C:\libpostal\` on Windows.

The compiled .dll will be in the `src/.libs/` directory and should be called `libpostal-1.dll`.

If you require a .lib import library to link this to your application. You can generate one using the Visual Studio `lib.exe` tool and the `libpostal.def` definition file:
```
lib.exe /def:libpostal.def /out:libpostal.lib /machine:x64
```

Installation with an alternative data model
-------------------------------------------

An alternative data model is available for libpostal. It is created by Senzing Inc. for improved parsing on US, UK and Singapore addresses and improved US rural route address handling.
To enable this add `MODEL=senzing` to the conigure line during installation:
```
./configure --datadir=[...some dir with a few GB of space...] MODEL=senzing
```

The data for this model is gotten from [OpenAddress](https://openaddresses.io/), [OpenStreetMap](https://www.openstreetmap.org/) and data generated by Senzing based on customer feedback (a few hundred records), a total of about 1.2 billion records of data from over 230 countries, in 100+ languages. The data from OpenStreetMap and OpenAddress is good but not perfect so the data set was modified by filtering out badly formed addresses, correcting misclassified address tokens and removing tokens that didn't belong in the addresses, whenever these conditions were encountered.

Senzing created a data set of 12950 addresses from 89 countries that it uses to test and verify the quality of its models. The data set was generated using random addresses from OSM, minimally 50 per country. Hard-to-parse addresses were gotten from Senzing support team and customers and from the libpostal github page and added to this set. The Senzing model got 4.3% better parsing results than the default model, using this test set.

The size of this model is about 2.2GB compared to 1.8GB for the default model so keep that in mind if storages space is important.

Further information about this data model can be found at: https://github.com/Senzing/libpostal-data
If you run into any issues with this model, whether they have to do with parses, installation or any other problems, then please report them at https://github.com/Senzing/libpostal-data

Examples of parsing
-------------------

libpostal's international address parser uses machine learning (Conditional Random Fields) and is trained on over 1 billion addresses in every inhabited country on Earth. We use [OpenStreetMap](https://openstreetmap.org) and [OpenAddresses](https://openaddresses.io) as sources of structured addresses, and the OpenCage address format templates at: https://github.com/OpenCageData/address-formatting to construct the training data, supplementing with containing polygons, and generating sub-building components like apartment/floor numbers and PO boxes. We also add abbreviations, drop out components at random, etc. to make the parser as robust as possible to messy real-world input.

These example parse results are taken from the interactive address_parser program 
that builds with libpostal when you run ```make```. Note that the parser can handle
commas vs. no commas as well as various casings and permutations of components (if the input
is e.g. just city or just city/postcode).

![parser](https://cloud.githubusercontent.com/assets/238455/24703087/acbe35d8-19cf-11e7-8850-77fb1c3446a7.gif)

The parser achieves very high accuracy on held-out data, currently 99.45%
correct full parses (meaning a 1 in the numerator for getting *every* token
in the address correct).

Usage (parser)
--------------

Here's an example of the parser API using the Python bindings:

```python

from postal.parser import parse_address
parse_address('The Book Club 100-106 Leonard St Shoreditch London EC2A 4RH, United Kingdom')
```

And an example with the C API:

```c
#include <stdio.h>
#include <stdlib.h>
#include <libpostal/libpostal.h>

int main(int argc, char **argv) {
    // Setup (only called once at the beginning of your program)
    if (!libpostal_setup() || !libpostal_setup_parser()) {
        exit(EXIT_FAILURE);
    }

    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();
    libpostal_address_parser_response_t *parsed = libpostal_parse_address("781 Franklin Ave Crown Heights Brooklyn NYC NY 11216 USA", options);

    for (size_t i = 0; i < parsed->num_components; i++) {
        printf("%s: %s\n", parsed->labels[i], parsed->components[i]);
    }

    // Free parse result
    libpostal_address_parser_response_destroy(parsed);

    // Teardown (only called once at the end of your program)
    libpostal_teardown();
    libpostal_teardown_parser();
}
```

Parser labels
-------------

The address parser can technically use any string labels that are defined in the training data, but these are the ones currently defined, based on the fields defined in [OpenCage's address-formatting library](https://github.com/OpenCageData/address-formatting), as well as a few added by libpostal to handle specific patterns:

- **house**: venue name e.g. "Brooklyn Academy of Music", and building names e.g. "Empire State Building"
- **category**: for category queries like "restaurants", etc.
- **near**: phrases like "in", "near", etc. used after a category phrase to help with parsing queries like "restaurants in Brooklyn"
- **house_number**: usually refers to the external (street-facing) building number. In some countries this may be a compount, hyphenated number which also includes an apartment number, or a block number (a la Japan), but libpostal will just call it the house_number for simplicity.
- **road**: street name(s)
- **unit**: an apartment, unit, office, lot, or other secondary unit designator
- **level**: expressions indicating a floor number e.g. "3rd Floor", "Ground Floor", etc.
- **staircase**: numbered/lettered staircase
- **entrance**: numbered/lettered entrance
- **po_box**: post office box: typically found in non-physical (mail-only) addresses
- **postcode**: postal codes used for mail sorting
- **suburb**: usually an unofficial neighborhood name like "Harlem", "South Bronx", or "Crown Heights"
- **city_district**: these are usually boroughs or districts within a city that serve some official purpose e.g. "Brooklyn" or "Hackney" or "Bratislava IV"
- **city**: any human settlement including cities, towns, villages, hamlets, localities, etc.
- **island**: named islands e.g. "Maui"
- **state_district**: usually a second-level administrative division or county.
- **state**: a first-level administrative division. Scotland, Northern Ireland, Wales, and England in the UK are mapped to "state" as well (convention used in OSM, GeoPlanet, etc.)
- **country_region**: informal subdivision of a country without any political status
- **country**: sovereign nations and their dependent territories, anything with an [ISO-3166 code](https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2).
- **world_region**: currently only used for appending “West Indies” after the country name, a pattern frequently used in the English-speaking Caribbean e.g. “Jamaica, West Indies”

Examples of normalization
-------------------------

The expand_address API converts messy real-world addresses into normalized
equivalents suitable for search indexing, hashing, etc. 

Here's an interactive example using the Python binding:

![expand](https://cloud.githubusercontent.com/assets/238455/14115012/52990d14-f5a7-11e5-9797-159dacdf8c5f.gif)

libpostal contains an OSM-trained language classifier to detect which language(s) are used in a given
address so it can apply the appropriate normalizations. The only input needed is the raw address string. 
Here's a short list of some less straightforward normalizations in various languages.

| Input                               | Output (may be multiple in libpostal)   |
| ----------------------------------- |-----------------------------------------|
| One-hundred twenty E 96th St        | 120 east 96th street                    |
| C/ Ocho, P.I. 4                     | calle 8 polígono industrial 4           |
| V XX Settembre, 20                  | via 20 settembre 20                     |
| Quatre vingt douze R. de l'Église   | 92 rue de l eglise                      |
| ул Каретный Ряд, д 4, строение 7    | улица каретныи ряд дом 4 строение 7     |
| ул Каретный Ряд, д 4, строение 7    | ulitsa karetnyy ryad dom 4 stroyeniye 7 |
| Marktstraße 14                      | markt strasse 14                        |

libpostal currently supports these types of normalizations in *60+ languages*,
and you can [add more](https://github.com/openvenues/libpostal/tree/master/resources/dictionaries) (without having to write any C).

For further reading and some bizarre address edge-cases, see:
[Falsehoods Programmers Believe About Addresses](https://www.mjt.me.uk/posts/falsehoods-programmers-believe-about-addresses/).

Usage (normalization)
---------------------

Here's an example using the Python bindings for succinctness (most of the higher-level language bindings are similar):

```python
from postal.expand import expand_address
expansions = expand_address('Quatre-vingt-douze Ave des Champs-Élysées')

assert '92 avenue des champs-elysees' in set(expansions)
```

The C API equivalent is a few more lines, but still fairly simple:

```c
#include <stdio.h>
#include <stdlib.h>
#include <libpostal/libpostal.h>

int main(int argc, char **argv) {
    // Setup (only called once at the beginning of your program)
    if (!libpostal_setup() || !libpostal_setup_language_classifier()) {
        exit(EXIT_FAILURE);
    }

    size_t num_expansions;
    libpostal_normalize_options_t options = libpostal_get_default_options();
    char **expansions = libpostal_expand_address("Quatre-vingt-douze Ave des Champs-Élysées", options, &num_expansions);

    for (size_t i = 0; i < num_expansions; i++) {
        printf("%s\n", expansions[i]);
    }

    // Free expansions
    libpostal_expansion_array_destroy(expansions, num_expansions);

    // Teardown (only called once at the end of your program)
    libpostal_teardown();
    libpostal_teardown_language_classifier();
}
```

Command-line usage (expand)
---------------------------

After building libpostal:

```
cd src/

./libpostal "Quatre vingt douze Ave des Champs-Élysées"
```

If you have a text file or stream with one address per line, the command-line interface also accepts input from stdin:

```
cat some_file | ./libpostal --json
```

Command-line usage (parser)
---------------------------

After building libpostal:

```
cd src/

./address_parser
```

address_parser is an interactive shell. Just type addresses and libpostal will
parse them and print the result.


Bindings
--------

Libpostal is designed to be used by higher-level languages.  If you don't see your language of choice, or if you're writing a language binding, please let us know!

**Officially supported language bindings**

- Python: [pypostal](https://github.com/openvenues/pypostal)
- Ruby: [ruby_postal](https://github.com/openvenues/ruby_postal)
- Go: [gopostal](https://github.com/openvenues/gopostal)
- Java/JVM: [jpostal](https://github.com/openvenues/jpostal)
- PHP: [php-postal](https://github.com/openvenues/php-postal)
- NodeJS: [node-postal](https://github.com/openvenues/node-postal)
- R: [poster](https://github.com/ironholds/poster)

**Unofficial language bindings**

- Java: [javacpp-presets-libpostal](https://github.com/bytedeco/javacpp-presets/tree/master/libpostal)
- LuaJIT: [lua-resty-postal](https://github.com/bungle/lua-resty-postal)
- Perl: [Geo::libpostal](https://metacpan.org/pod/Geo::libpostal)
- Elixir: [Expostal](https://github.com/SweetIQ/expostal)
- Rust: [rustpostal](https://crates.io/crates/rustpostal)

**Database extensions**

- PostgreSQL: [pgsql-postal](https://github.com/pramsey/pgsql-postal)

**Unofficial REST API**

- Libpostal REST: [libpostal REST](https://github.com/johnlonganecker/libpostal-rest)

**Libpostal REST Docker**

- Libpostal REST Docker [Libpostal REST Docker](https://github.com/johnlonganecker/libpostal-rest-docker)

**Libpostal ZeroMQ Docker**

- Libpostal ZeroMQ Docker image: [pasupulaphani/libpostal-zeromq](https://hub.docker.com/r/pasupulaphani/libpostal-zeromq/) , Source: [Github](https://github.com/pasupulaphani/libpostal-docker) 


Tests
-----

libpostal uses [greatest](https://github.com/silentbicycle/greatest) for automated testing. To run the tests, use:

```
make check
```

Adding [test cases](https://github.com/openvenues/libpostal/tree/master/test) is easy, even if your C is rusty/non-existent, and we'd love contributions. We use mostly functional tests checking string input against string output.

libpostal also gets periodically battle-tested on millions of addresses from OSM (clean) as well as anonymized queries from a production geocoder (not so clean). During this process we use valgrind to check for memory leaks and other errors.

Data files
----------

libpostal needs to download some data files from S3. The basic files are on-disk
representations of the data structures necessary to perform expansion. For address
parsing, since model training takes a few days, we publish the fully trained model 
to S3 and will update it automatically as new addresses get added to OSM, OpenAddresses, etc. Same goes for the language classifier model.

Data files are automatically downloaded when you run make. To check for and download
any new data files, you can either run ```make```, or run:

```
libpostal_data download all $YOUR_DATA_DIR/libpostal
```

And replace $YOUR_DATA_DIR with whatever you passed to configure during install.

Language dictionaries
---------------------

libpostal contains a number of per-language dictionaries that influence expansion, the language classifier, and the parser. To explore the dictionaries or contribute abbreviations/phrases in your language, see [resources/dictionaries](https://github.com/openvenues/libpostal/tree/master/resources/dictionaries).

Training data
-------------

In machine learning, large amounts of training data are often essential for getting good results. Many open-source machine learning projects either release only the model code (results reproducible if and only if you're Google), or a pre-baked model where the training conditions are unknown.

Libpostal is a bit different because it's trained on open data that's available to everyone, so we've released the entire training pipeline (the [geodata](https://github.com/openvenues/libpostal/tree/master/scripts/geodata) package in this repo), as well as the resulting training data itself on the Internet Archive. It's over 100GB unzipped.

Training data are stored on archive.org by the date they were created. There's also a file stored in the main directory of this repo called `current_parser_training_set` which stores the date of the most recently created training set. To always point to the latest data, try something like: ```latest=$(cat current_parser_training_set)``` and use that variable in place of the date.

### Parser training sets ###
All files can be found at https://archive.org/download/libpostal-parser-training-data-YYYYMMDD/$FILE as gzip'd tab-separated values (TSV) files formatted like:```language\tcountry\taddress```.

- **formatted_addresses_tagged.random.tsv.gz** (ODBL): OSM addresses. Apartments, PO boxes, categories, etc. are added primarily to these examples
- **formatted_places_tagged.random.tsv.gz** (ODBL): every toponym in OSM (even cities represented as points, etc.), reverse-geocoded to its parent admins, possibly including postal codes if they're listed on the point/polygon. Every place gets a base level of representation and places with higher populations get proportionally more.
- **formatted_ways_tagged.random.tsv.gz** (ODBL): every street in OSM (ways with highway=*, with a few conditions), reverse-geocoded to its admins
- **geoplanet_formatted_addresses_tagged.random.tsv.gz** (CC-BY): every postal code in Yahoo GeoPlanet (includes almost every postcode in the UK, Canada, etc.) and their parent admins. The GeoPlanet admins have been cleaned up and mapped to libpostal's tagset
- **openaddresses_formatted_addresses_tagged.random.tsv.gz** (various licenses, mostly CC-BY): most of the address data sets from [OpenAddresses](https://openaddresses.io/), which in turn come directly from government sources
- **uk_openaddresses_formatted_addresses_tagged.random.tsv.gz** (CC-BY): addresses from [OpenAddresses UK](https://alpha.openaddressesuk.org/)

If the parser doesn't perform as well as you'd hoped on a particular type of address, the best recourse is to use grep/awk to look through the training data and try to determine if there's some pattern/style of address that's not being captured.

Features
--------

- **Abbreviation expansion**: e.g. expanding "rd" => "road" but for almost any
language. libpostal supports > 50 languages and it's easy to add new languages
or expand the current dictionaries. Ideographic languages (not separated by
whitespace e.g. Chinese) are supported, as are Germanic languages where
thoroughfare types are concatenated onto the end of the string, and may
optionally be separated so Rosenstraße and Rosen Straße are equivalent.

- **International address parsing**: [Conditional Random Field](http://blog.echen.me/2012/01/03/introduction-to-conditional-random-fields/) which parses
"123 Main Street New York New York" into {"house_number": 123, "road":
"Main Street", "city": "New York", "state": "New York"}. The parser works
for a wide variety of countries and languages, not just US/English. 
The model is trained on over 1 billion addresses and address-like strings, using the
templates in the [OpenCage address formatting repo](https://github.com/OpenCageData/address-formatting) to construct formatted,
tagged traning examples for every inhabited country in the world. Many types of [normalizations](https://github.com/openvenues/libpostal/blob/master/scripts/geodata/addresses/components.py)
are performed to make the training data resemble real messy geocoder input as closely as possible.

- **Language classification**: multinomial logistic regression
trained (using the [FTRL-Proximal](https://research.google.com/pubs/archive/41159.pdf) method to induce sparsity) on all of OpenStreetMap ways, addr:* tags, toponyms and formatted
addresses. Labels are derived using point-in-polygon tests for both OSM countries
and official/regional languages for countries and admin 1 boundaries
respectively. So, for example, Spanish is the default language in Spain but
in different regions e.g. Catalunya, Galicia, the Basque region, the respective 
regional languages are the default. Dictionary-based disambiguation is employed in
cases where the regional language is non-default e.g. Welsh, Breton, Occitan.
The dictionaries are also used to abbreviate canonical phrases like "Calle" => "C/"
(performed on both the language classifier and the address parser training sets)

- **Numeric expression parsing** ("twenty first" => 21st, 
"quatre-vingt-douze" => 92, again using data provided in CLDR), supports > 30
languages. Handles languages with concatenated expressions e.g.
milleottocento => 1800. Optionally normalizes Roman numerals regardless of the
language (IX => 9) which occur in the names of many monarchs, popes, etc.

- **Fast, accurate tokenization/lexing**: clocked at > 1M tokens / sec,
implements the TR-29 spec for UTF8 word segmentation, tokenizes East Asian
languages chracter by character instead of on whitespace.

- **UTF8 normalization**: optionally decompose UTF8 to NFD normalization form,
strips accent marks e.g. à => a and/or applies Latin-ASCII transliteration.

- **Transliteration**: e.g. улица => ulica or ulitsa. Uses all
[CLDR transforms](http://www.unicode.org/repos/cldr/trunk/common/transforms/), the exact same source data as used by [ICU](http://site.icu-project.org/),
though libpostal doesn't require pulling in all of ICU (might conflict 
with your system's version). Note: some languages, particularly Hebrew, Arabic
and Thai may not include vowels and thus will not often match a transliteration 
done by a human. It may be possible to implement statistical transliterators
for some of these languages.

- **Script detection**: Detects which script a given string uses (can be
multiple e.g. a free-form Hong Kong or Macau address may use both Han and
Latin scripts in the same address). In transliteration we can use all
applicable transliterators for a given Unicode script (Greek can for instance
be transliterated with Greek-Latin, Greek-Latin-BGN and Greek-Latin-UNGEGN).

Non-goals
---------

- Verifying that a location is a valid address
- Actually geocoding addresses to a lat/lon (that requires a database/search index)

Raison d'être
-------------

libpostal was originally created as part of the [OpenVenues](https://github.com/openvenues/openvenues) project to solve the problem of venue deduping. In OpenVenues, we have a data set of millions of
places derived from terabytes of web pages from the [Common Crawl](http://commoncrawl.org/).
The Common Crawl is published monthly, and so even merging the results of
two crawls produces significant duplicates.

Deduping is a relatively well-studied field, and for text documents 
like web pages, academic papers, etc. there exist pretty decent approximate
similarity methods such as [MinHash](https://en.wikipedia.org/wiki/MinHash).

However, for physical addresses, the frequent use of conventional abbreviations
such as Road == Rd, California == CA, or New York City == NYC complicates
matters a bit. Even using a technique like MinHash, which is well suited for
approximate matches and is equivalent to the Jaccard similarity of two sets, we
have to work with very short texts and it's often the case that two equivalent
addresses, one abbreviated and one fully specified, will not match very closely
in terms of n-gram set overlap. In non-Latin scripts, say a Russian address and
its transliterated equivalent, it's conceivable that two addresses referring to
the same place may not match even a single character.

As a motivating example, consider the following two equivalent ways to write a
particular Manhattan street address with varying conventions and degrees
of verbosity:

- 30 W 26th St Fl #7
- 30 West Twenty-sixth Street Floor Number 7

Obviously '30 W 26th St Fl #7 != '30 West Twenty-sixth Street Floor Number 7'
in a string comparison sense, but a human can grok that these two addresses
refer to the same physical location.

libpostal aims to create normalized geographic strings, parsed into components,
such that we can more effectively reason about how well two addresses
actually match and make automated server-side decisions about dupes.

So it's not a geocoder?
-----------------------

If the above sounds a lot like geocoding, that's because it is in a way,
only in the OpenVenues case, we have to geocode without a UI or a user 
to select the correct address in an autocomplete dropdown. Given a database 
of source addresses such as OpenAddresses or OpenStreetMap (or all of the above), 
libpostal can be used to implement things like address deduping and server-side
batch geocoding in settings like MapReduce or stream processing.

Now, instead of trying to bake address-specific conventions into traditional
document search engines like Elasticsearch using giant synonyms files, scripting,
custom analyzers, tokenizers, and the like, geocoding can look like this:

1. Run the addresses in your database through libpostal's expand_address
2. Store the normalized string(s) in your favorite search engine, DB, 
   hashtable, etc.
3. Run your user queries or fresh imports through libpostal and search
   the existing database using those strings

In this way, libpostal can perform fuzzy address matching in constant time
relative to the size of the data set.

Why C?
------

libpostal is written in C for three reasons (in order of importance):

1. **Portability/ubiquity**: libpostal targets higher-level languages that
people actually use day-to-day: Python, Go, Ruby, NodeJS, etc. The beauty of C
is that just about any programming language can bind to it and C compilers are
everywhere, so pick your favorite, write a binding, and you can use libpostal
directly in your application without having to stand up a separate server. We
support Mac/Linux (Windows is not a priority but happy to accept patches), have
a standard autotools build and an endianness-agnostic file format for the data
files. The Python bindings, are maintained as part of this repo since they're
needed to construct the training data.

2. **Memory-efficiency**: libpostal is designed to run in a MapReduce setting
where we may be limited to < 1GB of RAM per process depending on the machine
configuration. As much as possible libpostal uses contiguous arrays, tries
(built on contiguous arrays), bloom filters and compressed sparse matrices to
keep memory usage low. It's possible to use libpostal on a mobile device with
models trained on a single country or a handful of countries.

3. **Performance**: this is last on the list for a reason. Most of the
optimizations in libpostal are for memory usage rather than performance.
libpostal is quite fast given the amount of work it does. It can process
10-30k addresses / second in a single thread/process on the platforms we've
tested (that means processing every address in OSM planet in a little over
an hour). Check out the simple benchmark program to test on your environment
and various types of input. In the MapReduce setting, per-core performance
isn't as important because everything's being done in parallel, but there are
some streaming ingestion applications at Mapzen where this needs to
run in-process.

C conventions
-------------

libpostal is written in modern, legible, C99 and uses the following conventions:

- Roughly object-oriented, as much as allowed by C
- Almost no pointer-based data structures, arrays all the way down
- Uses dynamic character arrays (inspired by [sds](https://github.com/antirez/sds)) for safer string handling
- Confines almost all mallocs to *name*_new and all frees to *name*_destroy
- Efficient existing implementations for simple things like hashtables
- Generic containers (via [klib](https://github.com/attractivechaos/klib)) whenever possible
- Data structrues take advantage of sparsity as much as possible
- Efficient double-array trie implementation for most string dictionaries
- Cross-platform as much as possible, particularly for *nix

Preprocessing (Python)
----------------------

The [geodata](https://github.com/openvenues/libpostal/tree/master/scripts/geodata) Python package in the libpostal repo contains the pipeline for preprocessing the various geo
data sets and building training data for the C models to use.
This package shouldn't be needed for most users, but for those interested in generating new types of addresses or improving libpostal's training data, this is where to look.

Address parser accuracy
-----------------------

On held-out test data (meaning labeled parses that the model has _not_ seen
before), the address parser achieves 99.45% full parse accuracy.

For some tasks like named entity recognition it's preferable to use something
like an F1 score or variants, mostly because there's a class bias problem (most
words are non-entities, and a system that simply predicted non-entity for
every token would actually do fairly well in terms of accuracy). That is not
the case for address parsing. Every token has a label and there are millions
of examples of each class in the training data, so accuracy is preferable as it's
a clean, simple and intuitive measure of performance.

Here we use full parse accuracy, meaning we only give the parser one "point" in
the numerator if it gets every single token in the address correct. That should
be a better measure than simply looking at whether each token was correct.

Improving the address parser
----------------------------

Though the current parser works quite well for most standard addresses, there
is still room for improvement, particularly in making sure the training data
we use is as close as possible to addresses in the wild. There are two primary
ways the address parser can be improved even further (in order of difficulty):

1. Contribute addresses to OSM. Anything with an addr:housenumber tag will be
   incorporated automatically into the parser next time it's trained.
2. If the address parser isn't working well for a particular country, language
   or style of address, chances are that some name variations or places being
   missed/mislabeled during training data creation. Sometimes the fix is to
   update the formats at: https://github.com/OpenCageData/address-formatting,
   and in many other cases there are relatively simple tweaks we can make
   when creating the training data that will ensure the model is trained to
   handle your use case without you having to do any manual data entry.
   If you see a pattern of obviously bad address parses, the best thing to
   do is post an issue to Github.

Contributing
------------

Bug reports, issues and pull requests are welcome. Please read the [contributing guide](CONTRIBUTING.md) before submitting your issue, bug report, or pull request.

Submit issues at: https://github.com/openvenues/libpostal/issues.


Shoutouts
---------

Special thanks to @BenK10 for the initial Windows build and @AeroXuk for integrating it seamlessly into the project and setting up an Appveyor build.

License
-------

The software is available as open source under the terms of the [MIT License](http://opensource.org/licenses/MIT).
