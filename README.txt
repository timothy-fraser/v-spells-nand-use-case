
Emulated NAND Flash Storage Device Driver Use Case

Copyright (c) 2022 Provatek, LLC.

        This material is based upon work supported by the
        Defense Advanced Research Projects Agency (DARPA)
        and Naval Information Warfare Center Pacific (NIWC
        Pacific) under Contract No. N66001-21-C-4026. Any
        opinions, findings and conclusions or
        recommendations expressed in this material are those
        of the author(s) and do not necessarily reflect the
        views of DARPA or NIWC Pacific.

        In the event permission is required, DARPA is
        authorized to reproduce the copyrighted material for
        use as an exhibit or handout at DARPA-sponsored
        events and/or to post the material on the DARPA
        website.

        This software distribution is ** Approved for Public
        Release, Distribution Unlimited. ** (DISTAR Case
        #37059.)

This Emulated NAND Flash Storage Device Driver Use Case software
distribution provides a test rig, corpus, and documentation to support
experimentation with the piecewise replacement of individual
components of large legacy software systems with new versions that
posess formally-verified correctness properties.

Provatek, LLC originally developed it to support the Defense Advanced
Research Projects Agency (DARPA)'s Verified Security and Performance
Enhancement of Large Legacy Software (V-SPELLS) research program.
V-SPELLS explored novel techniques and tools for the extraction of
specifications from legacy software in terms specific to their
application domain, the creation of Domain-Specific Languages (DSLs)
tailored to those application domains, and the use of those DSLs to
reimplement components in a way that was compatible with the extracted
specificiations and had additional cybersecurity and performance
improvements.

This use case provided the V-SPELLS research teams with a small
user-mode C program that emulated a corpus of raw NAND Flash storage
device drivers, some correctly demonstrating patterns typically found
in real Linux drivers, some demonstrating bugs.  It invited them to
extract the specifications for the upward-facing interface the drivers
shared with an emulated NAND framework and the downward-facing
interface they shared with an emulated NAND storage device, design a
DSL tailored to driver implementation, reimplement the drivers in that
DSL, and demonstrate that the new DSL-based drivers respected both
interfaces while mitigating bugs.

While Provatek, LLC originally developed the use case to support the
V-SPELLS DSL-oriented experiments, it seems equally applicable to
Model-Based Design approaches that seek to extract domain-specific
models from legacy software and to synthesize correct component
reimplementations from those models.  Researchers will find it
contains a number of interesting features, including domain-specific
concepts such as raw NAND Flash erase blocks and programmable pages,
Input/Output registers, and General-Purpose Input Output pins.  It
also contains interfaces with varied semantics, such as
message-passing protocols with state machines, function call through
jump tables, an interpreted command language, and a complex data
structure shared between components.

Provatek, LLC hopes that researchers will find this use case to be a
convenient platform for their experiments and a useful common corpus
for comparing different techniques and tools.  Further information can
be found in the manual included in the distribution.


Contents of this distribution:

README.txt - this file.
manual.pdf - the user manual for this distro.  Start here.
LICENSE    - the open source license for this distro and its docs.

Bugs       - a database of bugs for the last month of work on the test
             rig.  Fixed bugs are in Bugs/Fixed.  Open bugs are in
             Bugs/High, Bugs/Medium, and Bugs/Low, organized by
             priority.

Makefile   - Makefile for building the test rig and its test programs.

device, driver, framework, tester, main - these directories hold the
source for each of the test rig's components.

