/* Prime number generator, run as:
**
**     bc primes.b
**
** requires gnu bc
*/


ignore = scale(0);

define primes (low, high) {
    auto p, i;

    if (low < 5) low = 5;
    if (low % 2 == 0) low = low - 1;
    if (high % 2 == 0) high = high + 1;

    print "\nPrimes from ", low, " to ", high, "\n";

    for (p=low; p <= high; p += 2)  {
        isprime = 1;
        for (i = 2; i < (p/2); i++) {
            if ((p % i) == 0) {
                isprime = 0;
                break;
            }
        }
        if (isprime) print "\t", p, "\n";
    }
}

print "\nSpecify bottom of range (eg 20): ";
bot = read();
print "Specify top of range (eg 90): ";
top = read();

ignore = primes(bot, top);

quit
