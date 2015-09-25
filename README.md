     ___        __                               __             ___      
    /\_ \    __/\ \                             /\ \__         /\_ \     
    \//\ \  /\_\ \ \____  _____     ___     ____\ \ ,_\    __  \//\ \    
      \ \ \ \/\ \ \ '__`\/\ '__`\  / __`\  /',__\\ \ \/  /'__`\  \ \ \   
       \_\ \_\ \ \ \ \L\ \ \ \L\ \/\ \L\ \/\__, `\\ \ \_/\ \L\.\_ \_\ \_ 
       /\____\\ \_\ \_,__/\ \ ,__/\ \____/\/\____/ \ \__\ \__/.\_\/\____\
       \/____/ \/_/\/___/  \ \ \/  \/___/  \/___/   \/__/\/__/\/_/\/____/
                            \ \_\                                        
                             \/_/                                        
    ---------------------------------------------------------------------

**N.B.**: libpostal is not publicly released yet and the APIs may change. We
encourage folks to hold off on including it as a dependency for now.
Stay tuned...

libpostal is a fast, multilingual, all-i18n-everything NLP library for 
normalizing and parsing physical addresses.

Addresses and the geographic coordinates they represent are essential for any
location-based application (map search, transportation, on-demand/delivery
services, check-ins, reviews). Yet even the simplest addresses are packed with
local conventions, abbreviations and context, making them difficult to
index/query effectively with traditional full-text search engines, which are
designed for document indexing. This library helps convert the free-form
addresses that humans use into clean normalized forms suitable for machine
comparison and full-text indexing.

libpostal is not itself a full geocoder, but should be a ubiquitous
preprocessing step before indexing/searching with free text geographic strings.
It is written in C for maximum portability and performance.

libpostal's raison d'être
-------------------------

libpostal was created as part of the [OpenVenues](https://github.com/openvenues/openvenues) project to solve 
the problem of place deduping. In OpenVenues, we have a data set of millions of
places derived from terabytes of web pages from the [Common Crawl](http://commoncrawl.org/).
The Common Crawl is published every month, and so even merging the results of
two crawls produces significant duplicates.

Deduping is a relatively well-studied field, and for text documents like web
pages, academic papers, etc. we've arrived at pretty decent approximate
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

libpostal aims to create normalized geographic strings, parsed into components,
such that we can more effectively reason about how well two addresses
actually match.

As a motivating example, consider the following two equivalent ways to write a
particular Manhattan street address with varying conventions and degrees
of verbosity:

- 30 W 26th St Fl #7
- 30 West Twenty-sixth Street Floor Number 7

Obviously '30 W 26th St Fl #7 != '30 West Twenty-sixth Street Floor Number 7'
in a string comparison sense, but a human can grok that these two addresses
refer to the same physical location.

Isn't that geocoding?
---------------------

If the above sounds a lot like geocoding, that's because it's very similar,
only in the OpenVenues case, we do it without a UI or a user to select the
correct address in an autocomplete. It's server-side batch geocoding
(and you can too!)

Now, instead of giant Elasticsearch synonyms files, etc.
geocoding can look like this:

1. Run the addresses in your index through libpostal
2. Store the canonical strings
3. Run your user queries through libpostal and search with those strings

Features
--------

- **Abbreviation expansion**: e.g. expanding "rd" => "road" but for almost any
language. libpostal supports > 50 languages and it's easy to add new languages
or expand the current dictionaries. Ideographic languages (not separated by
whitespace e.g. Chinese) are supported, as are Germanic languages where
thoroughfare types are concatenated onto the end of the string, and may
optionally be separated so Rosenstraße and Rosen Straße are equivalent.

- **International address parsing (coming soon)**: sequence model which parses
"123 Main Street New York New York" into {"house_number": 123, "road":
"Main Street", "city": "New York", "region": "New York"}. Unlike the majority
of parsers out there, it works for a wide variety of countries and languages,
not just US/English. The model is trained on > 40M OSM addresses, using the
templates in the [OpenCage address formatting repo](https://github.com/OpenCageData/address-formatting) to construct formatted,
tagged traning examples for most countries around the world.

- **Language classification (coming soon)**: multinomial logistic regression
trained on all of OpenStreetMap ways, addr:* tags, toponyms and formatted
addresses. Labels are derived using point-in-polygon tests in Quattroshapes
and official/regional languages for countries and admin 1 boundaries
respectively. So, for example, Spanish is the default language in Spain but
in different regions e.g. Catalunya, Galicia, the Basque region, regional
languages are the default. Dictionary-based disambiguation is employed in
cases where the regional language is non-default e.g. Welsh, Breton, Occitan.

- **Numeric expression parsing** ("twenty first" => 21st, 
"quatre-vignt-douze" => 92, again using data provided in CLDR), supports > 30
languages. Handles languages with concatenated expressions e.g.
milleottocento => 1800. Optionally normalizes Roman numerals regardless of the
language (IX => 9) which occur in the names of many monarchs, popes, etc.

- **Geographic name aliasing**: New York, NYC and Nueva York alias to New York
City. Uses the crowd-sourced GeoNames (geonames.org) database, so alternate
names added by contributors can automatically improve libpostal.

- **Geographic disambiguation (coming soon)**: There are several equally
likely Springfields in the US (formally known as The Simpsons problem), and
some context like a state is required to disambiguate. There are also > 1200
distinct San Franciscos in the world but the term "San Francisco" almost always
refers to the one in California. Williamsburg can refer to a neighborhood in
Brooklyn or a city in Virginia. Geo disambiguation is a subset of Word Sense
Disambiguation, and attempts to resolve place names in a string to GeoNames
entities. This can be useful for city-level geocoding suitable for polygon/area
lookup. By default, if there is no other context, as in the San Francisco case,
the most populous entity will be selected.

- **Ambiguous token classification (coming soon)**: e.g. "dr" => "doctor" or
"drive" for an English address depending on the context. Multiclass logistic
regression trained on OSM addresses, where abbreviations are discouraged,
giving us many examples of fully qualified addresses on which to train.

- **Fast, accurate tokenization/lexing**: clocked at > 1M tokens / sec,
implements the TR-29 spec for UTF8 word segmentation, tokenizes East Asian
languages chracter by character instead of on whitespace.

- **UTF8 normalization**: optionally decompose UTF8 to NFD normalization form,
strips accent marks e.g. à => a and/or apply Latin-ASCII transliteration.

- **Transliteration**: e.g. улица => ulica or ulitsa. Uses all
[CLDR transforms][http://www.unicode.org/repos/cldr/trunk/common/transforms/], which is what ICU uses,
but libpostal doesn't require pulling in all of ICU (possibly conflicting with
your system's version). Note: some languages, particularly Hebrew, Arabic
and Thai may not include vowels andthus will not often match a transliteration 
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
- Street-level geocoding

Examples of expansion
---------------------

Like many problems in information extraction and NLP, address normalization
may sound trivial initially, but in fact can be quite complicated in real
natural language texts. Here are some examples of the kinds of address-specific
challenges libpostal can handle:

| Input                               | Output                                |
| ----------------------------------- |---------------------------------------|
| One-hundred twenty E 96th St        | 120 east 96th street                  |
| C/ Ocho, P.I. 4                     | calle 8, polígono industrial 4        |
| V XX Settembre, 20                  | via 20 settembre, 20                  |
| Quatre vignt douze Rue de l'Église  | 92 rue de l' église                   |
| ул Каретный Ряд, д 4, строение 7    | улица каретныи ряд, дом 4, строение 7 |
| ул Каретный Ряд, д 4, строение 7    | ulica karetnyj rad, dom 4, stroenie 7 |
| Marktstrasse 14                     | markt straße 14                       |

For further reading and some less intuitive examples of addresses, see
"[Falsehoods Programmers Believe About Addresses](https://www.mjt.me.uk/posts/falsehoods-programmers-believe-about-addresses/)".

Why C (you crazy person)?
-------------------------

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
keep memory usage low. It's conceivable that libpostal could even be used on
a mobile device, although that's not an explicit goal of the project.

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

Design philosophy
-----------------

libpostal is written in modern, legible, C99. 

- Keep it object-oriented(-ish)
- Confine almost all mallocs to *name*_new and all frees to *name*_destroy
- Don't write custom hashtables, sorting algorithms, other undergrad CS stuff
- Use generic containers from klib where possible
- Take advantage of sparsity in all data structures
- Use char_array (inspired by [sds](https://github.com/antirez/sds)) when possible instead of C strings.
- Throughly test for memory leaks before pushing
- Keep it reasonably cross-platform compatible, particularly for *nix

Language dictinonaries
----------------------

It's easy to add new languages/synonyms to libpostal by modifying a few text
files. The format of each dictionary file roughly resembles a
Lucene/Elasticsearch synonyms file:

```
drive|dr
street|st|str
road|rd
```

The leftmost string is treated as the canonical/normalized version. Synonyms
if any, are appended to the right, delimited by the pipe character.

The supported languages can be found in the [resources/dictionaries](https://github.com/openvenues/libpostal/tree/master/resources/dictionaries).

Each language can define one or more dictionaries (sometimes called "gazetteers" in NLP) to help with address parsing, and normalizing abbreviations. The dictionary types are:

- **academic_degrees.txt**: for post-nominal strings like "M.D.", "Ph.D.", etc.
- **ambiguous_expansions.txt**: e.g. "E" could be expanded to "East" or could
be "E Street", so if the string it encountered, it can either be left alone or expanded
- **building_types.txt**: strings indicating a building/house
- **company_types.txt**: company suffixes like "Inc" or "GmbH"
- **concatenated_prefixes_separable.txt**: things like "Hinter..." which can
be written either concatenated or as separate tokens
- **concatenated_suffixes_inseparable.txt**: Things like "...bg." => "...burg"
where the suffix cannot be separated from the main token, but either has an
abbreviated equivalent or simply can help identify the token in parsing as,
say, part of a street name
- **directionals.txt**: strings indicating directions (cardinal and
lower/central/upper, etc.)
- **level_types.txt**: strings indicating a particular floor
- **no_number.txt**: strings like "no fixed address"
- **nulls.txt**: strings meaning "not applicable"
- **personal_suffixes.txt**: post-nominal suffixes, usually generational
like Jr/Sr
- **personal_titles.txt**: civilian, royal and military titles
- **place_names.txt**: strings found in names of places e.g. "theatre",
"aquarium", "restaurant". See [Nominatim Special Phrases](http://wiki.openstreetmap.org/wiki/Nominatim/Special_Phrases)
- **post_office.txt**: strings like "p.o. box"
- **qualifiers.txt**: strings like "township"
- **stopwords.txt**: prepositions and articles mostly, very common words
which may be ignored in some contexts
- **street_types.txt**: words like "street", "road", "drive" which indicate
a thoroughfare and their respective abbreviations.
- **synonyms.txt**: any miscellaneous synonyms/abbreviations e.g. "bros"
expands to "brothers", etc. These have no special meaning and will essentially
just be treated as string replacement.
- **toponyms.txt**: abbreviations for certain abbreviations relating to
toponyms like regions, places, etc. Note: GeoNames covers most of these.
In most cases better to leave these alone
- **unit_types.txt**: strings indicating an apartment or unit number

Most of the dictionaries have been derived with the following process:

1. Tokenize all the streets in OSM for a particular language
2. Count the words
3. Optionally use frequent item set mining to get frequent phrases
4. Run the most frequent words/phrases through Google Translate
5. Add the ones that mean "street" to dictionaries
6. Research thoroughfare types in a given country

In the future it might be beneficial to move the dictionaries to a wiki
so they can be crowdsourced by native speakers regardless of whether or not
they use git.

Installation
------------

For C users or those writing bindings (if you've written a languag
binding, please let us know!):

```
./bootstrap.sh
./configure --datadir=[...some dir with a few GB of space...]
make
sudo make install
```

libpostal needs to download some data files from S3. This is done automatically
when you run make. Mapzen maintains an S3 bucket containing said data files
but they can also be built manually.

To install via Python, just use:

```
pip install https://github.com/openvenues/libpostal.git
```

Command-line usage
------------------

After building libpostal:

```
cd src/

./libpostal "12 Three-hundred and forty-fifth ave, ste. no 678" en
#12 345th avenue, suite number 678

```

Currently libpostal requires two input strings, the address text and a language
code (ISO 639-1).

Todos
-----

1. Finish debugging/fully train address parser and publish model
2. Port language classification from Python, train and publish model
3. Python bindings and documentation
4. Publish tests (currently not on Github) and set up continuous integration
5. Hosted documentation