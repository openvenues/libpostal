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
- **level_types.txt**: strings indicating a particular floor
- **no_number.txt**: strings like "no fixed address"
- **nulls.txt**: strings meaning "not applicable"
- **personal_suffixes.txt**: post-nominal suffixes, usually generational
like Jr/Sr
- **personal_titles.txt**: civilian, royal and military titles
- **place_names.txt**: strings found in names of places e.g. "theatre",
"aquarium", "restaurant". [Nominatim Special Phrases](http://wiki.openstreetmap.org/wiki/Nominatim/Special_Phrases) is a great resource for this.
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