// vim:et:sts=8:ts=8:filetype=c

object $suite: $base_suite;

public method .name {
    return "Math";
};

public method .setup {
    pass();
};

// begin tests

public method .meta_test_b{
    .assertEquals("0".b(), 0);
    .assertEquals("1".b(), 1);
    .assertEquals("10".b(), 2);
};
