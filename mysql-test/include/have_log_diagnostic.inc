let $have_log_diagnostic = `SHOW VARIABLES LIKE 'log_diagnostic_enable'`;
if (!$have_log_diagnostic) {
    --skip "The test needs log_diagnostic_enable."
}

