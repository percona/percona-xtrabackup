# If we are running with ASAN then the resource consumption (storage space needed, runtime ...) will be extreme.
# Therefore we require that the option "--big-test" is also set.

let $have_asan = `SELECT VERSION() LIKE '%asan%'`;
if ($have_asan)
{
  if (!$BIG_TEST)
  {
    --skip Need '--big-test' or '--only-big-test' when running with ASAN
  }
}
