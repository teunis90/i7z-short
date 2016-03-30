# i7z-short
Read actual Intel CPU clockrate trough `/dev/cpu/<cpu_id>/msr` and return avarage clockrate per socket. Debug mode shows actual clockrate per core.

Run as root:

    ./i7z
    ./i7z -d (debug mode)
    
Output:

    <avg clockrate metric socket0> [<avg clockrate metric socket1>]
    
Tested CPU Types:

    Intel(R) Xeon(R) CPU E5-2620 v3 @ 2.40GHz
    Intel(R) Xeon(R) CPU E5-2640 0 @ 2.50GHz
    Intel(R) Xeon(R) CPU           L5640  @ 2.27GHz
    Intel(R) Xeon(R) CPU E5-2690 v3 @ 2.60GHz


Tested Operating Systems:

    CentOS Linux release 7.2.1511 (Core)
    CentOS release 6.5 (Final)
    CentOS release 6.6 (Final)
    CentOS release 6.7 (Final)

Code is based on: https://github.com/ajaiantilal/i7z (Abhishek Jaiantilal)
