// vim:et:sts=8:ts=8:filetype=c

object $suite: $base_suite;

public method .name() {
    return "System";
};

// begin tests

public method .should_not_create_object_without_parents {
    var obj;

    catch ~type {
        obj = create([]);
    } with {
        return;
    }
    .fail("create() with empty parent list did not throw error");
};
