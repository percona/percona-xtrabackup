
Replace resources     
********************************************************************************

Replace resources are collections of replace directives that follow common
rules:

- all placeholders use lower alphabet characters, the dot (.), and the dash (-)
  characters.
- all replace directives are arranged in the alphabetic order (descending)
- all replace directives in one file have a prefix that denotes the
  type of concept that they represent. The usage of prefix is
  consistent within one file: if a prefix is applicable then each
  placeholder must have it.

To avoid confusion, prefixes look similar to the concepts that they represents
but not as full words that may appear in text.

===========  ========   ========================================================
Collection   Prefix     Description
===========  ========   ========================================================
abbr         abbr.      Abbreviations
common       -          Common names the spelling of which is imporant 
file         fs.        File system objects: paths, directories, and files
parameter    param.     Parameters, options, and configuration variables
command      cmd.      Executable 
proper       -          Proper names such as company names
text         -          Fragments of text
===========  ========   ========================================================
