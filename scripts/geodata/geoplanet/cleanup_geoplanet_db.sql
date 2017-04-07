-- Worldwide
    update places
    -- Aruba, Burundi, Nicaragua and Paraguay are listed as "Nationality"
    set place_type = "Country"
    where place_type = "Nationality";

    -- For postal_codes parented by something like "LandFeature" or "Estate" or "HistoricalCounty"
    -- or some other non-admin feature, set to the parent_id
    update postal_codes
    set parent_id = (select p_sub.parent_id from all_places p_sub where p_sub.id = postal_codes.parent_id)
    where parent_id in (
        select distinct pc.parent_id
        from postal_codes pc
        left join places p
            on pc.parent_id = p.id
        where p.id is null
    );

    -- Run again for non-admin features parented by non-admin features
    update postal_codes
    set parent_id = (select p_sub.parent_id from all_places p_sub where p_sub.id = postal_codes.parent_id)
    where parent_id in (
        select distinct pc.parent_id
        from postal_codes pc
        left join places p
            on pc.parent_id = p.id
        where p.id is null
    );

-- United Kingdom
    -- Make UK an abbreviation rather than a variant (as those aren't used)
    update aliases set name_type = "A"
    where id = 23424975 -- United Kingdom
    and name = "UK" and language = "ENG";

    -- City of London
    update places
    set place_type = "Town"
    where id = 12695806;

    -- Isle of Man
    update admins
    set country_code = "IM",
    country_id = 0
    where id = 23424847;

    update places
    set country_code = "IM",
    place_type = "Country"
    where id = 23424847;

    update admins
    set country_code = "IM",
    country_id = state_id, -- Isle of Man is a "state" in GeoPlanet
    state_id = county_id, -- States in Isle of Man are labeled counties
    county_id = 0
    where state_id = 23424847;

    update places
    set country_code = "IM",
    place_type = "State"
    where parent_id = 23424847;

    update places
    set country_code = "IM"
    where id in (select id from admins where country_code = "IM");

    update postal_codes set country_code = "IM"
    where parent_id in (select id from places where country_code = "IM");

    -- Guernsey
    update admins
    set country_code = "GG",
    country_id = 0
    where id = 23424827;

    update admins
    set country_code = "GG",
    country_id = county_id, -- Guernsey is a "county" in GeoPlanet
    state_id = local_admin_id, -- States in Guernsey are labeled local_admins
    county_id = 0,
    local_admin_id = 0
    where county_id = 23424827;

    update places
    set country_code = "GG",
    place_type = "Country"
    where id = 23424827;

    update places
    set country_code = "GG",
    place_type = "State"
    where parent_id = 23424827;

    update places
    set country_code = "GG"
    where id in (select id from admins where country_code = "GG");

    update postal_codes set country_code = "GG"
    where parent_id in (select id from places where country_code = "GG");

    -- Jersey
    update admins
    set country_code = "JE",
    country_id = 0
    where id = 23424857;

    update admins
    set country_code = "JE",
    country_id = county_id, -- Jersey is a "county" in GeoPlanet
    state_id = local_admin_id, -- States in Jersey are labeled local_admins
    county_id = 0,
    local_admin_id = 0
    where county_id = 23424857;

    update places
    set country_code = "JE",
    place_type = "Country"
    where id = 23424857;

    update places
    set country_code = "JE",
    place_type = "State"
    where parent_id = 23424857;

    update places
    set country_code = "JE"
    where id in (select id from admins where country_code = "JE");

    update postal_codes set country_code = "JE"
    where parent_id in (select id from places where country_code = "JE");

    -- Postal codes assigned to a unitary authority should use the city
    update postal_codes
    set parent_id = (
        select sub.id
        from places parent
        join places sub
            on sub.parent_id = parent.id
        where parent.id = postal_codes.parent_id
        and sub.place_type = "Town"
        and parent.place_type = "LocalAdmin"
        and replace(parent.name, " City", "") = sub.name
        limit 1
    )
    where parent_id in (
        select distinct parent.id
        from places parent
        join places sub
            on sub.parent_id = parent.id
        where parent.country_code = "GB"
        and parent.name like "% City"
        and sub.place_type = "Town"
        and parent.place_type = "LocalAdmin"
        and replace(parent.name, " City", "") = sub.name
    );

    -- shire districts with City in the name are cities
    update places
    set place_type = "Town"
    where country_code = "GB"
    and place_type = "LocalAdmin"
    and name like  "% City" or name like "City %"
    -- except for City of Westminster in London
    and parent_id != 44418;

    -- shire districts 
    update places
    set place_type = "County"
    where country_code = "GB"
    and place_type = "LocalAdmin"
    -- except for boroughs of London
    and parent_id != 44418;

    -- One place in the UK (Garrison) is parented by a postal_code
    update places
    set parent_id = 20078335 -- Fermanagh
    where parent_id = 26353948; -- BT93 4


-- Canada

    -- Alberta
    -- listead as both county and state
    update places
    set parent_id = 2344915
    where parent_id = 29375228;

    update admins
    set county_id = 0
    where state_id = 2344915;

    -- Manitoba
    -- listead as both county and state
    update places
    set parent_id = 2344917
    where parent_id = 29375231;

    update admins
    set county_id = 0
    where state_id = 2344917;

    -- Newfoundland and Labrador
    -- listead as both county and state
    update places
    set parent_id = 2344919
    where parent_id = 29375216;

    update admins
    set county_id = 0
    where state_id = 2344919;

    -- Northwest Territories
    -- listed as both county and state
    update places
    set parent_id = 2344920
    where parent_id = 29375229;

    update admins
    set county_id = 0
    where state_id = 2344920;

    -- Québec
    -- listed as both county and state
    update postal_codes
    set parent_id = 2344924
    where parent_id = 29375121;

    update places
    set parent_id = 2344924
    where parent_id = 29375121;

    update admins
    set county_id = 0
    where county_id = 29375121;

    -- Saskatchewan
    -- listed as both county and state
    update places
    set parent_id = 2344925
    where parent_id = 29375232;

    update admins
    set county_id = 0
    where state_id = 2344925;

    -- Yukon Territory
    -- listed as both county and state
    update places
    set parent_id = 2344926
    where parent_id = 29375157;

    update admins
    set county_id = 0
    where state_id = 2344926;

    -- Nunavut
    -- listed as both county and state
    update places
    set parent_id = 20069920
    where parent_id = 29375230;

    update admins
    set county_id = 0
    where state_id = 20069920;

-- Portugal
    -- "County" in GeoPlanet is admin_level=7 (municipio) in OSM, switch to Town
    update places
    set place_type = "Town"
    where country_code = "PT"
    and place_type = "County";

    -- there are now Towns parented by Towns, so make their parent_id point to their parent's parent
    -- Note: SQLite can only do correlated subqueries, no joined updates, hence the clunky syntax
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (select p1.id from places p1 join places p2 on p1.parent_id = p2.id 
                 where p1.country_code = "PT" and p1.place_type = "Town" and p2.place_type = "Town");

    -- "State" in GeoPlanet is admin_level=6 in OSM, switch to County
    -- except for Azores and Madeira
    update places
    set place_type = "County"
    where country_code = "PT"
    and place_type = "State"
    and id not in (15021776, 2346570);

    update admins
    set county_id = state_id
    where country_code = "PT"
    and state_id not in (15021776, 2346570);

    update admins
    set county_id = 0
    where country_code = "PT"
    and state_id in (15021776, 2346570);

    update admins
    set state_id = 0
    where country_code = "PT"
    and state_id not in (15021776, 2346570);

-- Japan
    update places
    set place_type = "LocalAdmin"
    where country_code = "JP"
    and name like "%区";

    update places
    set place_type = "Suburb"
    where country_code = "JP"
    and name like "%丁目";

-- United States
    -- DC
    -- listed as both county and state
    update places
    set parent_id = 2347567
    where parent_id = 12587802;

    update admins
    set county_id = 0
    where state_id = 2347567;

    -- Counties outside of Louisiana should be $name County
    update places
    set name = printf("%s County", name)
    where country_code = "US"
    and place_type = "County"
    and name not like "% City"
    and parent_id not in (2347577, 2347560);

    -- Counties outside of Louisiana should be $name County
    update places
    set name = printf("%s Parish", name)
    where country_code = "US"
    and place_type = "County"
    and parent_id != 2347577;

    -- Preferred name for New York
    update aliases
    set name_type = "P"
    where id = 2459115
    and name = "New York City"
    and language = "ENG";

    -- Additional abbreviations for NYC
    update aliases
    set name_type = "A"
    where id = 2459115
    and name in ("NY City", "NY Cty", "New York Cty")
    and language = "ENG";

    -- Boroughs of NYC
    -- listed as counties, make them LocalAdmin
    update places
    set place_type = "LocalAdmin"
    where id in (
        12589314, -- Bronx
        12589335, -- Brooklyn
        12589342, -- Manhattan
        12589352, -- Queens
        12589354 -- Staten Island
    );

    update admins
    set local_admin_id = county_id,
    county_id = 0
    where county_id in (
        12589314, -- Bronx
        12589335, -- Brooklyn
        12589342, -- Manhattan
        12589352, -- Queens
        12589354 -- Staten Island
    );

-- Germany

    -- "LocalAdmin" level are usually Gemeindes (municipalities) when they are parented by counties
    -- and city districts when parented by towns
    update places
    set place_type = "Town"
    where country_code = "DE"
    and place_type = "LocalAdmin"
    and parent_id in (select id from places where country_code = "DE" and place_type = "County");


-- India - OK

-- France
    -- "LocalAdmin" level are cities except for the Arondissements
    -- of Paris, Marseille, and Lyon
    update places
    set place_type = "Town"
    where country_code = "FR"
    and place_type = "LocalAdmin"
    and parent_id not in (
        615702, -- Paris
        610264, -- Marseille
        609125 -- Lyon
    );

-- Poland
    -- "LocalAdmin" level is either gmina (municipality) or city in OSM.
    -- Since the "gmina" prefix is not used, we'll say city
    update places
    set place_type = "Town"
    where country_code = "PL"
    and place_type = "LocalAdmin";

-- Sweden
    -- "County" level is admin_level=7 in OSM (kommun) which are municipalities
    update places
    set place_type = "Town"
    where country_code = "SE"
    and place_type = "County";

    -- fix Towns parented by Towns
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (select p1.id from places p1 join places p2 on p1.parent_id = p2.id 
                 where p1.country_code = "SE" and p1.place_type = "Town" and p2.place_type = "Town");

    update admins
    set county_id = 0
    where country_code = "SE";

-- Brasil
    -- Many admin levels in Brasil, but in GeoPlanet "County" simply repeats the city name
    update places
    set place_type = "Town"
    where country_code = "BR"
    and place_type = "County";

    -- fix Towns parented by Towns
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (select p1.id from places p1 join places p2 on p1.parent_id = p2.id 
                 where p1.country_code = "BR" and p1.place_type = "Town" and p2.place_type = "Town");

    update admins
    set county_id = 0
    where country_code = "BR";

-- Romania - OK

-- Spain
    update places
    set place_type = "Town"
    where country_code = "ES"
    and place_type = "LocalAdmin";

-- Taiwan
    update places
    set place_type = "Town"
    where country_code = "TW"
    and place_type = "State"
    and name like "%市";

    -- Places with place_type=Town ending in 鎮 should be city_district
    update places
    set place_type = "LocalAdmin"
    where country_code = "TW"
    and place_type = "Town"
    and name like "%鎮"
    and parent_id in (select id from places where country_code = "TW" and place_type = "Town");


    -- update cities within top-level cities to be city_district (ending with 區/qu, not 市/shi)
    update places
    set place_type = "LocalAdmin",
    name = printf("%s區", substr(name, 1, length(name) - 1))
    where country_code = "TW"
    and place_type = "Town"
    and name like "%市"
    -- top-level cities in Taiwan
    and parent_id in (select id from places where parent_id = 23424971 and name like "%市");


    -- Places ending with qu (區) should be city_district
    update places
    set place_type = "LocalAdmin"
    where country_code = "TW"
    and place_type = "County"
    and name like "%區";

    -- Places ending with xiàn (縣) should be state_district
    update places
    set place_type = "County"
    where country_code = "TW"
    and place_type = "State"
    and name like "%縣";

    -- Places ending with xiāng (鄉) should be city
    update places
    set place_type = "Town"
    where country_code = "TW"
    and place_type = "County"
    and name like "%鄉";

    -- Places ending with lǐ (里) should be suburb
    update places
    set place_type = "Suburb"
    where country_code = "TW"
    and place_type = "County"
    and name like "%里";

-- Italy
    update places
    set place_type = "Town"
    where country_code = "IT"
    and place_type = "LocalAdmin";

-- Netherlands
    -- boroughs of Amsterdam should be city_district
    update places
    set place_type = "LocalAdmin"
    where id in (
        734698, -- Westpoort
        727281 -- Amsterdam Zuidoost
    );

    -- municipalities (cities) in OSM
    update places
    set place_type = "Town"
    where country_code = "NL"
    and place_type = "County";

-- Norway
    -- suburbs of Oslo
    update places
    set place_type = "LocalAdmin"
    where country_code = "NO"
    and place_type = "County"
    and parent_id = 862592;

    -- the rest are municipalities (cities) in OSM
    update places
    set place_type = "Town"
    where country_code = "NO"
    and place_type = "County"
    and parent_id in (select id from places where country_code = "NO" and place_type = "State");

-- Turkey
    -- Istanbul (city)
    update places
    set parent_id = 2347289
    where id = 2344116;

    update admins
    set county_id = 0
    where id = 2344116;

    -- districts of Istanbul
    update places
    set parent_id = 2344116, -- Istanbul (city)
    place_type = "LocalAdmin"
    where parent_id = 2347289 -- Istanbul (province)
    and place_type = "County";

-- Bulgaria
    -- municipalities (cities) in OSM
    update places
    set place_type = "Town"
    where country_code = "BG"
    and place_type = "County";

    update admins
    set county_id = 0
    where country_code = "BG";

-- Argentina - OK

-- Indonesia
    
    -- Set sub-municipalities of Jakaarta (e.g. Jakarta Barat) to city
    update places
    set place_type = "Town"
    where parent_id = 1047378 -- Jakarta (city)
    and place_type = "County";

-- Luxembourg
    update places
    set name = printf("District de %s", name)
    where country_code = "LU"
    and place_type = "State";

    update places
    set name = printf("Canton de %s", name)
    where country_code = "LU"
    and place_type = "County";

    -- Set suburbs of Luxembourg (city) to city_district
    update places
    set place_type = "LocalAdmin"
    where parent_id = 979721  -- Luxembourg City
    and place_type = "Suburb";

    -- Postal codes assigned to a LocalAdmin with a coterminous city should use the city
    update postal_codes
    set parent_id = (
        select p2.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.id = postal_codes.parent_id
        and p1.place_type = "LocalAdmin"
        and p2.place_type = "Town"
        and p1.name = p2.name
        limit 1
    )
    where parent_id in (
        select distinct p1.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.country_code = "LU"
        and p1.place_type = "LocalAdmin"
        and p2.place_type = "Town"
        and p1.name = p2.name
    );

    update places
    set place_type = "Town"
    where country_code = "LU"
    and parent_id != 979721  -- Luxembourg City
    and place_type = "LocalAdmin";

-- Switzerland
    -- using postal codes for Zürich the city
    update postal_codes
    set parent_id = 784794 -- Zürich (city)
    where parent_id = 12593130; -- Zürich (county)

    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "CH"
        and p1.place_type = "Town"
        and p2.place_type = "LocalAdmin"
    );

    update places
    set place_type = "Town"
    where country_code = "CH"
    and place_type = "LocalAdmin";

-- Australia - OK

-- Finland
    -- "LocalAdmin" level is city in OSM.
    update places
    set place_type = "Town"
    where country_code = "FI"
    and place_type = "LocalAdmin";

-- Czech Republic
    -- Use the prefix okres for Czech counties
    update places
    set name = printf("okres %s", name)
    where country_code = "CZ"
    and place_type = "County";

    -- LocalAdmins used here don't appear to have a corresponding type in OSM
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "CZ"
        and p1.place_type = "Town"
        and p2.place_type = "LocalAdmin"
    );

    update places
    set place_type = "Town"
    where country_code = "CZ"
    and place_type = "LocalAdmin";

-- Hungary
    -- Set Budapest's parent_id to the state
    update places
    set parent_id = 12577915
    where id = 804365;

    -- Set districts of Budapest to city_district, parented by Budapest the city
    update places
    set parent_id = 804365,
    place_type = "LocalAdmin"
    where parent_id = 12577915
    and place_type = "County";

    -- These are suburbs in OSM
    update places
    set place_type = "Suburb"
    where parent_id = 12593336 -- Bátonyterenye
    and place_type = "Town";

    -- Set all other counties to town
    update places
    set place_type = "Town"
    where country_code = "HU"
    and place_type = "County";

-- Algeria - OK

-- South Africa
    -- these are municipalities/cities
    update places
    set place_type = "Town"
    where country_code = "ZA"
    and place_type = "LocalAdmin";

-- Malaysia
    -- LocalAdmins used here don't appear to have a type in OSM
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "MY"
        and p1.place_type = "Town"
        and p2.place_type = "LocalAdmin"
    );

    update places
    set place_type = "Town"
    where id = 1140856; -- Bayan Lepas

    update places
    set parent_id = 1141153, -- George Town
    place_type = "Suburb"
    where parent_id = 56013581
    and id != 1141153 -- except George Town itself
    and place_type = "Town";

    update postal_codes
    set parent_id = 1149014 -- Kampong Sungai Gelugor
    where parent_id = 1149059; -- Kampong Sungai Keluang

    update postal_codes
    set parent_id = 1155026 -- Petaling Jaya
    where parent_id = 56013632; -- Petaling (county)

    update postal_codes
    set parent_id = (
        select p2.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.id = postal_codes.parent_id
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
        limit 1
    )
    where parent_id in (
        select distinct p1.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.country_code = "MY"
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
    );

-- Austria
    -- Set Vienna's parent to the state
    update places
    set parent_id = 2344716
    where id = 551801;

    -- Use the prefix Bezirk for Austrian counties
    update places
    set name = printf("Bezirk %s", name)
    where country_code = "AT"
    and place_type = "County";


    -- Postal codes assigned to a LocalAdmin with a coterminous city should use the city
    update postal_codes
    set parent_id = (
        select p2.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.id = postal_codes.parent_id
        and p1.place_type = "LocalAdmin"
        and p2.place_type = "Town"
        and p1.name = p2.name
        limit 1
    )
    where parent_id in (
        select distinct p1.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.country_code = "AT"
        and p1.place_type = "LocalAdmin"
        and p2.place_type = "Town"
        and p1.name = p2.name
    );

    -- Towns parented by a LocalAdmin should be parented by the grandparent County
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "AT"
        and p1.place_type = "Town"
        and p2.place_type = "LocalAdmin"
    );

    -- Convert all other LocalAdmins to cities
    update places
    set place_type = "Town"
    where country_code = "AT"
    and place_type = "LocalAdmin";

    -- Except the few districts/boroughs of Vienna listed in GeoPlanet
    update places
    set place_type = "LocalAdmin"
    where id in (542098, 551778);

-- China
    -- special cities that have state status
    update places
    set name = replace(name, "直辖市", "市"),
    place_type = "Town"
    where id in (
        12578011, -- Beijing
        12578012, -- Shanghai
        12578017, -- Tianjin
        20070171  -- Chongqing
    );

    -- City districts should be directly parented by their city
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "CN"
        and p1.place_type = "LocalAdmin"
        and p2.place_type = "County"
        and p2.parent_id in (
            12578011, -- Beijing
            12578012, -- Shanghai
            12578017, -- Tianjin
            20070171  -- Chongqing
        )
    );

    -- City districts should be directly parented by their city
    update places
    set parent_id = (select grandparent.parent_id from places parent join places grandparent on parent.parent_id = grandparent.id where parent.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        join places p3
            on p2.parent_id = p3.id
        where p1.country_code = "CN"
        and p1.place_type = "LocalAdmin"
        and p3.parent_id in (
            12578011, -- Beijing
            12578012, -- Shanghai
            12578017, -- Tianjin
            20070171  -- Chongqing
        )
    );

    -- GeoPlanet has 4 digit postcodes. They're correct but Chine uses 6 digits and pads with zeros
    update postal_codes
    set name = printf("%s00", name)
    where country_code = "CN"
    and length(name) = 4;

    -- LocalAdmin ending with shi (市) should be city
    update places
    set place_type = "Town"
    where country_code = "CN" 
    and name like "%市"
    and place_type in ("County", "LocalAdmin");

    -- Prefecture-level cities are labeled counties
    update places
    set place_type = "Town"
    where country_code = "CN"
    and place_type = "County"
    and replace(name, " ", "") not like "%自治州";


    -- Counties are labeled LocalAdmin
    update places
    set place_type = "County"
    where country_code = "CN" 
    and place_type = "LocalAdmin"
    and name like "%县";


-- New Zealand
    -- Hokianga Harbour is listed as a bay in OSM and these "suburbs" are villages around the bay
    update places
    set place_type = "Town"
    where place_type = "Suburb"
    and parent_id = 28645523;

    -- Silverdale listed as town in OSM
    update places
    set place_type = "Town"
    where id = 2350555;

    -- Wellington is both a city and a region
    update places
    set name = "Wellington Region"
    where id = 15021762;

-- Philippines
    -- States in GeoPlanet are admin_level=3 (country_region) in libpostal
    update places
    set place_type = "CountryRegion"
    where country_code = "PH"
    and place_type = "State";

    -- Counties in GeoPlanet are admin_level=4 (state) in libpostal
    update places
    set place_type = "State"
    where country_code = "PH"
    and place_type = "County";

-- Pakistan - OK

-- Lebanon - OK

-- Lithuania
    -- LocalAdmins are admin_level=6 (state_district)
    update places
    set place_type = "County"
    where country_code = "LT"
    and place_type = "LocalAdmin";

    -- Suburbs are admin_level=10 (city_district)
    update places
    set place_type = "LocalAdmin"
    where country_code = "LT"
    and place_type = "Suburb";

-- Estonia
    -- Counties in GeoPlanet are municipalities/cities
    update places
    set place_type = "Town"
    where country_code = "EE"
    and place_type = "County";

-- Slovakia
    -- Use the prefix okres for Slovak counties
    update places
    set name = printf("okres %s", name)
    where country_code = "SK"
    and place_type = "County"
    and id not in (
        29399347, -- Bratislava
        29399357 -- Košice
    );

    update places
    set place_type = "Town"
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "SK"
        and p1.place_type = "LocalAdmin"
        and p2.place_type = "County"
    );

-- Bangladesh
    -- only one postal code assigned to a district, so assign to town
    update postal_codes
    set parent_id = 1915034 -- Cox's Bazar (city)
    where parent_id = 23706415; -- Cox's Bazar District

-- Moldova
    -- weirdly the name of the capital city is spelled wrong
    update places
    set name = "Chișinău"
    where id = 480793;

    -- name of the state of Chișinău
    update places
    set name = "Municipiul Chișinău"
    where id = 20069878;

    -- Bălţi also has a weird spelling
    update places
    set name = "Bălţi"
    where id = 480080;

    -- Change state name to include Municipiul
    update places
    set name = "Municipiul Bălți"
    where id = 20069873;

    update places
    set name = printf("raionul %s", name)
    where country_code = "MD"
    and place_type = "State"
    and id not in (
        20069878, -- Municipiul Chișinău
        20069873, -- Municipiul Bălți
        20069881  -- Gagauzia
    );

-- Denmark
    -- Counties are municipalities/cities
    update places
    set place_type = "Town"
    where country_code = "DK"
    and place_type = "County";

-- Greece
    -- Counties are municipalities/cities
    update places
    set place_type = "Town"
    where country_code = "GR"
    and place_type = "LocalAdmin";

    -- uses English names, add "Region" to the end
    update places
    set name = printf("%s Region", name)
    where country_code = "GR"
    and place_type = "County";

-- Belgium
    -- LocalAdmins are municipalities/cities
    update places
    set place_type = "Town"
    where country_code = "BE"
    and place_type = "LocalAdmin";

    -- Suburbs of Antwerp are city_districts
    update places
    set place_type = "LocalAdmin"
    where country_code = "BE"
    and place_type = "Suburb"
    and parent_id = 966591; -- Antwerp

-- Israel
    -- Add "District" to Haifa, Tel Aviv, and Jerusalem
    update places
    set name = printf("%s District", name)
    where country_code = "IL"
    and place_type = "State"
    and id in (
        2345794, -- Haifa
        2345795, -- Tel Aviv
        2345796  -- Jerusalem
    );

-- Kenya - OK

-- Cyprus - OK

-- Croatia
    -- Towns parented by Counties should be parented by State
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "HR"
        and p1.place_type = "Town"
        and p2.place_type = "County"
    );

-- Georgia - OK

-- Latvia - OK

-- Chile
    -- Región Metropolitana de Santiago
    update places
    set name = "Región Metropolitana de Santiago"
    where id = 2345029;

    -- other states should begin with "Región de"
    update places
    set name = printf("Región de %s", name)
    where country_code = "CL"
    and place_type = "State"
    and id != 2345029;

    -- Counties begin with "Provincia de"
    update places
    set name = printf("Provincia de %s", name)
    where country_code = "CL"
    and place_type = "County";

    -- LocalAdmins are cities
    update places
    set place_type = "Town"
    where country_code = "CL"
    and place_type = "LocalAdmin";

-- México
    -- "Counties" parented by Mexico City are city_districts
    update places
    set place_type = "LocalAdmin",
    parent_id = 116545
    where country_code = "MX"
    and place_type = "County"
    and parent_id = 2346272;

-- Tunisia - OK

-- Ecuador
    -- Counties should begin with "Cantón"
    update places
    set name = printf("Cantón %s", name)
    where country_code = "EC"
    and place_type = "County";

-- Thailand
    -- Set postal codes parented by Bangkok (state) to Bangkok (city)
    update postal_codes
    set parent_id = 1225448
    where parent_id = 2347165;

    -- Bangkok city districts
    update places
    set place_type = "LocalAdmin",
    parent_id = 1225448
    where parent_id = 2347165
    and place_type = "County";

-- Nepal
    -- LocalAdmins are state_districts
    update places
    set place_type = "County"
    where country_code = "NP"
    and place_type = "LocalAdmin";

-- Macedonia
    -- no states in Macedonia, only municipalities
    update places
    set place_type = "Town"
    where country_code = "MK"
    and place_type = "State";

-- Morocco
    -- LocalAdmins are state_districts
    update places
    set place_type = "County"
    where country_code = "MA"
    and place_type = "LocalAdmin";

-- Venezuela
    -- LocalAdmin
    update places
    set place_type = "Town"
    where country_code = "VE"
    and place_type = "LocalAdmin";

-- Belarus - OK

-- Slovenia
    -- States/Counties in Slovenia are just municipalities
    update places
    set place_type = "Town"
    where country_code = "SI"
    and place_type in ("State", "County");

-- Guatemala
    -- Counties in Guatemala are just municipalities
    update places
    set place_type = "Town"
    where country_code = "GT"
    and place_type = "County";

-- Bosnia and Herzegovina - OK

-- Armenia - OK

-- Jordan - OK

-- Paraguay
    -- Counties in Paraguay are just municipalities
    update places
    set place_type = "Town"
    where country_code = "PY"
    and place_type = "County";

-- Sri Lanka
    -- add the suffix "Province" to states/provinces
    update places
    set name = printf("%s Province", name)
    where country_code = "LK"
    and place_type = "State";

    -- add the suffix "District" to all districts
    update places
    set name = printf("%s District", name)
    where country_code = "LK"
    and place_type = "County";

-- Senegal - OK

-- Honduras
    -- Postal codes assigned to a County with a coterminous city should use the city
    update postal_codes
    set parent_id = (
        select p2.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.id = postal_codes.parent_id
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
        limit 1
    )
    where parent_id in (
        select distinct p1.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.country_code = "HN"
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
    );

-- Mozambique - OK

-- Iraq - OK

-- Iran - OK

-- El Salvador
    -- States should be prefixed with "Departamento de"
    update places
    set name = printf("Departamento de %s", name)
    where country_code = "SV"
    and place_type = "State";

    -- Assign postal codes that are part of counties to their coterminous towns
    update postal_codes
    set parent_id = (
        select p2.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.id = postal_codes.parent_id
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
        limit 1
    )
    where parent_id in (
        select distinct p1.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.country_code = "SV"
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
    );

    -- The rest of the Counties are towns
    update places
    set place_type = "Town"
    where country_code = "SV"
    and place_type = "County";

-- Uruguay - OK

-- Egypt - OK

-- Nigeria - OK

-- Sudan - OK

-- Kazhakstan - OK

-- South Korea
    -- Counties below cities are city_districts
    update places
    set place_type = "LocalAdmin"
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "KR"
        and p1.place_type = "County"
        and p2.place_type = "Town"
    );

    -- Set LocalAdmins ending with 동/dong to suburb
    update places
    set place_type = "Suburb"
    where country_code = "KR"
    and place_type = "LocalAdmin"
    and name like "%동";

-- Monaco
    -- City of Monaco is parented by the country
    update places
    set parent_id = 23424892
    where id = 483301;

    -- Set wards of Monaco to city_district
    update places
    set place_type = "LocalAdmin"
    where parent_id = 483301;

-- Dominican Republic - OK

-- Russia - OK

-- Kuwait - OK

-- Maldives - OK

-- Uzbekiztan - OK

-- Puerto Rico
    -- Assign postal codes that are part of counties to their coterminous towns
    update postal_codes
    set parent_id = (
        select p2.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.id = postal_codes.parent_id
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
        limit 1
    )
    where parent_id in (
        select distinct p1.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.country_code = "PR"
        and p1.place_type = "County"
        and p2.place_type = "Town"
        and p1.name = p2.name
    );

    -- The rest of the Counties are towns
    update places
    set place_type = "Town"
    where country_code = "PR"
    and place_type = "County";

    -- States are counties
    update places
    set place_type = "County"
    where country_code = "PR"
    and place_type = "State";

-- Costa Rica
    -- prefix states with "Provincia"
    update places
    set name = printf("Provincia %s", name)
    where country_code = "CR"
    and place_type = "State";

    -- prefix counties with "Cantón"
    update places
    set name = printf("Cantón %s", name)
    where country_code = "CR"
    and place_type = "County";

    -- Towns parented by LocalAdmins should be parented by counties
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "CR"
        and p1.place_type = "Town"
        and p2.place_type = "LocalAdmin"
    );

    -- The rest of the LocalAdmins are villages
    update places
    set place_type = "Town"
    where country_code = "CR"
    and place_type = "LocalAdmin";

-- Haiti - OK

-- Palestine
    -- Gaza / West Bank are country_region
    update places
    set place_type = "CountryRegion"
    where country_code = "PS"
    and place_type = "State";

-- Iceland - OK

-- Montegnegro
    -- add the prefix "Opština" to states
    update places
    set name = printf("Opština %s", name)
    where country_code = "ME"
    and place_type = "State";

-- Laos - OK

-- Faroe Islands
    -- add the suffix "sýsla" to states
    update places
    set name = printf("%s sýsla", name)
    where country_code = "FO"
    and place_type = "State";

-- Ethiopia
    -- Set Addis Ababa's parent to the state
    update places
    set parent_id = 56013543
    where id = 1313090; 

    -- Set zones of Addis Ababa to city_district parented by the city
    update places
    set place_type = "LocalAdmin",
    parent_id = 1313090
    where id in (
        56017368, -- Addis Ababa Zone 1
        56017369, -- Addis Ababa Zone 2
        56017370, -- Addis Ababa Zone 3
        56017371, -- Addis Ababa Zone 4
        56017372, -- Addis Ababa Zone 5
        56017373  -- Addis Ababa Zone 6
    );

-- Madagascar
    -- Set towns and counties in Madagascar to just be parented by the country itself
    update places
    set parent_id = 23424883
    where country_code = "MG"
    and place_type in ("Town", "County");

    -- Set Antananarivo postal codes to the city
    update postal_codes
    set parent_id = 1358594
    where parent_id = 2346150;

    -- All other counties in Madagascar in GeoPlanet are municipalities
    update places
    set place_type = "Town"
    where country_code = "MG"
    and place_type = "County";

-- Papua New Guinea - OK

-- Guinea Bissau
    update places
    set name = printf("Região de %s", name)
    where country_code = "GW"
    and place_type = "State";

-- Singapore
    update places
    set name = printf("%s Community Development Council", name)
    where country_code = "SG"
    and place_type = "State";

-- Bermuda - OK

-- Guinea
    -- Add prefix "Région de" to states
    update places
    set name = printf("Région de %s", name)
    where country_code = "GN"
    and place_type = "State";

    -- Add prefix "Préfecture de" to counties
    update places
    set name = printf("Préfecture de %s", name)
    where country_code = "GN"
    and place_type = "County";

-- Niger
    -- Add prefix "Région de" to states
    update places
    set name = printf("Région de %s", name)
    where country_code = "NE"
    and place_type = "State";

    -- Add prefix "Département de" to counties
    update places
    set name = printf("Département de %s", name)
    where country_code = "NE"
    and place_type = "County";

-- Ukraine - OK

-- Swaziland
    update places
    set name = printf("Inkhundla %s", name)
    where country_code = "SZ"
    and place_type = "County";

-- Vietnam - OK

-- Azerbaijan - OK

-- French Polynesia
    -- Counties in French Polynesia are municipalities
    update places
    set place_type = "Town"
    where country_code = "PF"
    and place_type = "County";

    -- States in French Polynesia are state_districts
    update places
    set place_type = "County"
    where country_code = "PF"
    and place_type = "State";

-- Kyrgyzstan
    -- add suffix "Oblast" to the states
    update places
    set name = printf("%s Oblast", name)
    where country_code = "KG"
    and place_type = "State";

-- Turkmenistan
    -- Towns should be parented by the country
    update places
    set parent_id = 23424972
    where country_code = "TM"
    and place_type = "Town";
    -- States should be cities
    update places
    set place_type = "Town"
    where country_code = "TM"
    and place_type = "State";

-- Brunei
    -- Counties are just cities/villages
    update places
    set place_type = "Town"
    where country_code = "BN"
    and place_type = "County";

-- Åland Islands - OK

-- Réunion - OK

-- Guadeloupe - OK

-- Cabo Verde - OK

-- Mongolia - OK

-- New Caledonia
    -- Set correct names for the provinces
    update places
    set name = "Province des Îles Loyauté"
    where id = 24549805;

    update places
    set name = "Province du Nord"
    where id = 24549806;

    update places
    set name = "Province Sud"
    where id = 24549807;

    -- Counties in New Caledonia are municipalities
    update places
    set place_type = "Town"
    where country_code = "NC"
    and place_type = "County";

-- Martinique
    -- Counties in Martinique are municipalities
    update places
    set place_type = "Town"
    where country_code = "MQ"
    and place_type = "County";


-- Greenland
    -- Towns in Greenland should just be parented by the country, states aren't current anyway
    update places
    set parent_id = 23424828
    where country_code = "GL"
    and place_type = "Town";

-- Malta
    -- Counties in Malta are municipalities
    update places
    set place_type = "Town"
    where country_code = "MT"
    and place_type = "County";

-- South Sudan - OK

-- French Guiana - OK

-- Ireland
    update places
    set name = printf("County %s", name)
    where country_code = "IE"
    and place_type = "State";

-- Guam
    update places
    set place_type = "Town"
    where country_code = "GU"
    and place_type = "State";

-- US Virgin Islands
    -- Towns parented by counties should be parented by the island/state
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "VI"
        and p1.place_type = "Town"
        and p2.place_type = "County"
    );

    -- GeoPlanet counties in the Virgin Islands are municipalities
    update places
    set place_type = "Town"
    where country_code = "VI"
    and place_type = "County";

    -- States in the Virgin Islands are US counties
    update places
    set place_type = "County"
    where country_code = "VI"
    and place_type = "State";

-- Oman
    -- Counties in Oman are municipalities
    update places
    set place_type = "Town"
    where country_code = "OM"
    and place_type = "County";

-- Liechtenstein
    -- States in Liechtenstein are municipalities (gemeinde)
    update places
    set place_type = "Town"
    where country_code = "LI"
    and place_type = "State";

-- Mayotte
    -- States in Mayotte are municipalities (gemeinde)
    update places
    set place_type = "Town"
    where country_code = "YT"
    and place_type = "State";

-- Bahrain
    -- add the suffix "محافظة" (governorate) to states
    update places
    set name = printf("%s محافظة", name)
    where country_code = "BH"
    and place_type = "State";

    -- Counties in Bahrain are municipalities
    update places
    set place_type = "Town"
    where country_code = "BH"
    and place_type = "County";

-- San Marino
    -- Città di San Marino
    update places
    set name = "Città di San Marino"
    where id = 532373;

    -- States in San Marino are municipalities
    update places
    set place_type = "Town"
    where country_code = "SM"
    and place_type = "State";

    -- Map postal codes to cities instead of states
    update postal_codes
    set parent_id = (
        select p2.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.id = postal_codes.parent_id
        and p1.place_type = "State"
        and p2.place_type = "Town"
        and p1.name = p2.name
        limit 1
    )
    where parent_id in (
        select distinct p1.id
        from places p1
        join places p2
            on p1.id = p2.parent_id
        where p1.country_code = "SM"
        and p1.place_type = "State"
        and p2.place_type = "Town"
        and p1.name = p2.name
    );

    -- Cities should be parented by the country, not the states
    update places
    set parent_id = (select p_sub.parent_id from places p_sub where p_sub.id = places.parent_id)
    where id in (
        select p1.id
        from places p1
        join places p2
            on p1.parent_id = p2.id
        where p1.country_code = "SM"
        and p1.place_type = "Town"
        and p2.place_type = "State"
    );

-- Timor Leste - OK

-- Zambia
    -- Add suffix "Province" on states
    update places
    set name = printf("%s Province", name)
    where country_code = "ZM"
    and place_type = "State";

-- Andorra
    -- Postal codes for Andorra la Vella should be on the city, not the state
    update postal_codes
    set parent_id = 472553 -- Andorra la Vella (city)
    where parent_id = 20070553; -- Andorra la Vella (state)

-- Federated States of Micronesia - OK

-- Northern Mariana Islands
    -- Add suffix "Municipality" and set states to counties
    update places
    set name = printf("%s Municipality", name),
    place_type = "County"
    where country_code = "MP"
    and place_type = "State";

-- Tajikistan - OK

-- Wallis-et-Futuna
    -- Cities should be parented by country, not states
    update places
    set parent_id = 23424989 -- Wallis-et-Futuna
    where id = 1064134; -- Matâ' Utu

-- Marshall Islands
    -- States in the Marshall Islands are US counties
    update places
    set place_type = "County"
    where country_code = "MH"
    and place_type = "State";

-- American Samoa
    -- Counties in American Samoa should be parented by the country
    update places
    set parent_id = 23424746 -- American Samoa
    where country_code = "AS"
    and place_type = "County";

-- Saint-Barthélemy - OK

-- Cocos (Keeling) Islands - OK

-- Christmas Island - OK

-- Norfolk Island - OK

-- Saint-Pierre-et-Miquelon
    -- Set all postal codes to the country
    update postal_codes
    set parent_id = 23424939 -- Saint-Pierre-et-Miquelon
    where country_code = "PM";

-- Palau - OK

-- US Minor Outlying Islands
    -- Counties should be parented by the country
    update places
    set place_type = "County"
    where country_code = "UM"
    and place_type = "State";

    -- Set all postal codes to the country
    update postal_codes
    set parent_id = 28289407 -- US Minor Outlying Islands
    where country_code = "UM";

-- Vatican - OK
