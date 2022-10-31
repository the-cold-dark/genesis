// vim:et:sts=8:ts=8:filetype=c
// Setup: create root and sys

object $main: $root;

public method .print_errors() {
    arg errors, suite;
    var error_count, error;

    error_count = listlen(errors);
    if (error_count > 0) {
        dblog("");
        dblog("The following tests failed (" + error_count + "):");
        for error in (errors) {
            dblog("---------------");
            dblog("Suite   : " + suite.name());
            dblog("Method  : " + error['method]);
            dblog("Code    : " + error['code]);
            dblog("Message : " + error['msg]);
            dblog("---------------");
            dblog(""+error['trace]);
        }
        // hard_exit_error();
    }
};

public method .run_suite {
    arg suite;
    var x, method, tc, trace, error_list, helpers, method_name;

    error_list = [];
    catch any {
        suite.setup();
        helpers = suite.helpers();
    } with {
        dblog(toliteral(traceback()));
    }

    tc = 2;
    for x in (suite.methods()) {
        catch any {
            method_name = lowercase(tostr(x));
            if ("test" in method_name || "should" in method_name) {
                (> suite.(x)() <);
                dblog(".");
            }
        } with {
            if (error() != ~success) { // fix this
                dblog("F");
                trace = traceback();
                method = trace[2][2];
                //step up from helper methods
                catch any {
                    while (method in (helpers)) {
                        tc += 1;
                        method = trace[tc][2];
                    }
                } with {
                    dblog(toliteral(traceback()));
                }
                error_list += [#[['method, method], ['code, trace[1][1]], ['msg, trace[1][2]], ['trace, trace]]];
            }
            continue;
        }
    }

    catch any {
        .print_errors(error_list, suite);
    } with {
        dblog("ERROR: " + toliteral(traceback()));
    }
};

eval {
    var errors;
    errors = .run_suite($suite);
    shutdown();
};
