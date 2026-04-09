# If we are running with UBSAN then the resource consumption (storage space needed, runtime ...) will be extreme.
# Therefore we require that the option "--big-test" is also set.

let $have_ubsan = `SELECT VERSION() LIKE '%ubsan%'`;
if ($have_ubsan)
{
  if (!$BIG_TEST)
  {
    --skip Need '--big-test' or '--only-big-test' when running with UBSAN
  }
}
