## Submitting Issues

When submitting issues to libpostal, please respect these guidelines:

- Be constructive. Try to help solve the problem.
- Always search for existing issues before submitting one.
- If you've written your own address parsing library/service, whether open-source or proprietary, don't raise issues simply to advertise for your project/solution. Write about it elsewhere, and save the issues page for people who are actually using libpostal.

### Bad parses

Libpostal's parser uses machine learning. It improves as the data improves, but contrary to the hype, that doesn't mean it can do everything a human brain can do. Addresses have many edge cases, and while we cover a substantial number of them, we may not be able to handle every bizarre edge case that comes up.

When reporting a parser issue, only submit one issue per problematic *pattern* of address, preferably with multiple addresses attached. For each address, please include at minimum:

- Input address
- Expected result
- Can you find the address in [OpenStreetMap](https://openstreetmap.org)?
- If libpostal is getting a place name like a city, suburb, or state wrong, can the admin component(s) name be found in OSM?
- What's the minimum form of the address that will parse correctly. For instance, if "123 Main St New York, NY" is the problem address, will "123 Main St" work? Does it work without abbreviations, using local language names, without sub-building information like units?

Note: we don't claim to handle all of the formatting mistakes that abound in address data sets, so sometimes the input needs to be preprocessed in some way before sending to libpostal. Sometimes there simply is no immediate solution, and many times the solution is simply to add your address or some part of it to OSM.

However, if there's a specific place or style of address that libpostal gets wrong, often we can do something to help libpostal train for and understand that address.


### Bugs

When submitting bug reports, please be sure to give us as much context as possible so that we can reproduce the error you encountered. Be sure to include:

- System conditons (OS, etc.)
- Steps to reproduce
- Expected outcome
- Actual outcome
- Screenshots or traceback
- Input or code that exposes the bug, if possible

