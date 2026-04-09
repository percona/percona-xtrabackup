# ==== Purpose ====
#
# Prepare the variable value in a way that i might be
# transfered in:
#
# * URL - query part
# * different command line envs as commands argument
#
# * $mrs_escape_query - value to be escaped
# * $mrs_include_character_equal - escape equal character
# * $mrs_include_character_question_mark - escape question mark character
# * $mrs_include_character_and - escape and character
#
# ==== Usage ====
#
# [--let $mrs_include_character_question_mark=1]
# [--let $mrs_include_character_equal=1]
# [--let $mrs_include_character_and=1]
# [--let $mrs_escape_query={"aaa": "b?"}]
# --source ../include/mrs/escape_url_query.inc
#

if (!$mrs_escape_query)
{
	--die The parameter `mrs_escape_query` is required.
}


--let $mrs_escape_query=`SELECT REPLACE(REPLACE($mrs_escape_query, '%', '%25'), '$', '%24')`
--let $mrs_escape_query=`SELECT REPLACE('$mrs_escape_query', '"', '%22')`
--let $mrs_escape_query=`SELECT REPLACE('$mrs_escape_query', '\r\n', '%0D%0A')`
--let $mrs_escape_query=`SELECT REPLACE('$mrs_escape_query', '\n', '%0A')`
--let $mrs_escape_query=`SELECT REPLACE(REPLACE('$mrs_escape_query', '{', '%7B'), '}', '%7D')`
--let $mrs_escape_query=`SELECT REPLACE(REPLACE('$mrs_escape_query', '(', '%28'), ')', '%29')`
--let $mrs_escape_query=`SELECT REPLACE(REPLACE('$mrs_escape_query', '[', '%5B'), ']', '%5D')`
--let $mrs_escape_query=`SELECT REPLACE('$mrs_escape_query', ' ', '%20')`

if ($mrs_include_character_equal)
{
--let $mrs_escape_query=`SELECT REPLACE('$mrs_escape_query', '=', '%3D')`
}
if ($mrs_include_character_question_mark)
{
--let $mrs_escape_query=`SELECT REPLACE('$mrs_escape_query', '?', '%3F')`
}
if ($mrs_include_character_and)
{
--let $mrs_escape_query=`SELECT REPLACE('$mrs_escape_query', '&', '%26')`
}

--let $mrs_include_character_equal=
--let $mrs_include_character_question_mark=
--let $mrs_include_character_and=
