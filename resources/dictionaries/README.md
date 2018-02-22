Language dictionaries
=====================

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
- **ambiguous_expansions.txt**: e.g. "E" could be expanded to "East" but could
be "E Street", so if the string is encountered, it can either be left alone or expanded. In general, single-letter abbreviations in most languages should also be added to ambiguous_expansions.txt since single letters are also often initials
- **building_types.txt**: strings indicating a building/house
- **categories.txt**: category strings e.g. from [Nominatim Special Phrases](http://wiki.openstreetmap.org/wiki/Nominatim/Special_Phrases) expected to be used in searches like "restaurants in Manhattan". Singular and plural forms can be included here.
- **company_types.txt**: company suffixes like "Inc" or "GmbH"
- **concatenated_prefixes_separable.txt**: things like "Hinter..." which can
be written either concatenated or as separate tokens
- **concatenated_suffixes_inseparable.txt**: Things like "...bg." => "...burg"
where the suffix cannot be separated from the main token, but either has an
abbreviated equivalent or simply can help identify the token in parsing as,
say, part of a street name
- **concatenated_suffixes_separable.txt**: Things like "...straße" where the
suffix can be either concatenated to the main token or separated
- **directionals.txt**: strings indicating directions (cardinal and
lower/central/upper, etc.)
- **entrance.txt**: string indicating an entrance, usually just the word "entrance" and its appropriate abbreviations.
- **given_names.txt**: a dictionary of given names derived from Wikipedia has been provided in the special "all" language directory. Western given names are usually abbreviated using the first letter but specifying them all would create too many potential variations. Instead of trying to expand "J" to every possible J name, it might be better to abbreviate possible given names and add those versions as alternate forms of the string for matching purposes.
- **house_number.txt**: strings that may be added as part of the house/building number (for languages like Spanish where it's common to say "No. 123" or "No. Ext. 123" for the house/building number instead of just "123" as in English).
- **level_types_basement.txt**: strings indicating a basement level.
- **level_types_mezzanine.txt**: strings indicating a mezzanine level.
- **level_types_numbered.txt**: strings indicating a numbered level of a building (numbered).
- **level_types_standalone.txt**: strings indicating a level/floor of a building that can stand on their own without a number like "ground floor", etc.
- **level_types_sub_basement.txt**: strings indicating a sub-basement level.
- **no_number.txt**: strings like "sin número" used for houses with no number.
- **nulls.txt**: strings meaning "not applicable" e.g. in spreadsheets or database fields that might have missing values
- **organizations.txt**: e.g. common retail chains, organizational acronyms for government agencies, the United Nations, etc.
- **people.txt**: abbreviations for specific people like MLK for Martin Luther King, Jr. or CDG for Charles du Galle
- **personal_suffixes.txt**: post-nominal suffixes, usually generational e.g. Junior/Senior in English or der Jungere in German.
- **personal_titles.txt**: civilian, royal, clerical, and military titles e.g. "Saint", "General", etc.
- **place_names.txt**: strings found in names of places e.g. "theatre",
"aquarium", "restaurant". [Nominatim Special Phrases](http://wiki.openstreetmap.org/wiki/Nominatim/Special_Phrases) is a great resource for this.
- **post_office.txt**: strings like "p.o. box"
- **qualifiers.txt**: strings like "township"
- **staircase.txt**: strings indicating a staircase, usually just the word "staircase" or "stair".
- **stopwords.txt**: prepositions and articles mostly, very common words
which may be ignored in some contexts
- **street_names.txt**: words which can be found in street names but are not thoroughfare types e.g. "spring" and their abbreviations. These would tend to be "core" words i.e. part of the base street name, although some can be thoroughfare types as well.
- **street_types.txt**: words like "street", "road", "drive" which indicate
a thoroughfare and their respective abbreviations.
- **surnames.txt**: a dictionary of surnames derived from Wikipedia has been provided in the special "all" language directory. If there are specific abbreviations for surnames in a language like Mdez. for Menendez, add them in the specific language's dictionary.
- **synonyms.txt**: any miscellaneous synonyms/abbreviations e.g. "bros"
expands to "brothers", etc. These have no special meaning and will essentially
just be treated as string replacement.
- **toponyms.txt**: abbreviations for certain abbreviations relating to
toponyms like regions, places, etc. Note: GeoNames covers most of these.
In most cases better to leave these alone
- **unit_directions.txt**: phrases to indicate which side of the building the apartment/unit is on, usually along the lines of "left", "right", "front", "rear".
- **unit_types_numbered.txt**: strings indicating a apartment or unit e.g. we expect a number to follow (or in some languages, precede) strings like "flat", "apt", "unit", etc.
- **unit_types_standalone.txt**: for unit type that can stand on their own without an accompanying number e.g. "penthouse".

Most of the dictionaries have been derived using the following process:

1. Tokenize every street/venue name in OSM for language x using libpostal
2. Count the most common tokens
3. Use the [Apriori algorithm](https://en.wikipedia.org/wiki/Apriori_algorithm) to extract multi-word phrases
4. Run the most frequent words/phrases through Google Translate
5. Add the ones that mean "street" (or other relevant words) to dictionaries
6. Augment by researching addresses in countries speaking language x

Contributing
------------

If you're a native speaker of one or more languages in libpostal, we'd love your contribution! It's as simple as editing the text files under this directory and submitting a pull request. Dictionaries are organized by [language code](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes), so feel free to find any language you speak and start editing! If you don't see your language, just add a directory - there's no additional configuration needed.

To get started adding new language dictionaries or improving support for existing languages, check out the [address_languages](https://github.com/openvenues/address_languages) repo, where we've published lists of 1-5 word phrases found in street/venue names in every language in OSM.

In the future it might be beneficial to move these dictionaries to a wiki
so they can be crowdsourced by native speakers regardless of whether or not
they use git.
