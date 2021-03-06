---
layout: index
title: Jool - Home
---

# Home

-------------------

## Introduction

Jool is an Open Source [SIIT and NAT64](intro-nat64.html) for Linux.

* [Click here](doc-index.html) to start getting acquainted with the software.
* [Click here](download.html) to download Jool.

-------------------

## Status

If you want to know what the development team is currently tweaking, you should always be able to answer that by having a look at the latest milestone - <a href="https://github.com/NICMx/NAT64/milestones" target="_blank">milestone list</a>.

But just to make things clear, this is the status of the project as of now:

As far as we know, Jool is a [fairly compliant](intro-jool.html#compliance) SIIT and Stateful NAT64. We're currently working on [switching frameworks](https://github.com/NICMx/NAT64/issues/116#issuecomment-74343381) to address our compliance issues.

Our latest release is version <a href="https://github.com/NICMx/NAT64/issues?q=milestone%3A3.3.0" target="_blank">3.3.0</a>.

-------------------

## News

### 2015-03-05

Jool 3.3.0 is finished.

[Filtering couldn't make it into the milestone](https://github.com/NICMx/NAT64/issues/41#issuecomment-76861510), but Stateless IP/ICMP Translation (SIIT) is now supported.

See the updated [SIIT/NAT64 introduction](intro-nat64.html) for an improved picture of the SIIT paradigm. [Here's the tutorial](mod-run-vanilla.html). Also keep an eye on [464XLAT](mod-run-464xlat.html).

We also refactored the userspace app somewhat; please review your scripts:

- The kernel's per-interface MTU setting [replaced `--minMTU6`](file:///home/aleiva/Desktop/NAT64/doc/usr/build/misc-mtu.html).
- `--address`, `--prefix`, `--bib4` and `--bib6` disappeared because they're considered redundant. See [`--pool6`](usr-flags-pool6.html), [`--pool4`](usr-flags-pool4.html) and [`--bib`](usr-flags-bib.html).

We also released Jool 3.2.3, which is [bugfixes](https://github.com/NICMx/NAT64/milestones/3.2.3) since 3.2.2. One of the bugs is a DoS vulnerability, so upgrading to at least 3.2.3 is highly recommended.

### 2014-10-24

An <a href="https://github.com/NICMx/NAT64/issues/112" target="_blank">important bug</a> was discovered, and version 3.2.2 is out.

### 2014-10-17

The documentation of `--plateaus` proved to be lacking, so there's now a [full article](usr-flags-plateaus.html) dedicated to it. The [original definition](usr-flags-global.html#mtu-plateaus) also received improvements.

It has come to our attention that <a href="https://github.com/NICMx/NAT64/issues/111" target="_blank">we are also lacking an explanation of IP literals</a>, so there should be another codeless update like this in the near future.

### 2014-10-08

Version 3.2.1 released. The 3.2 series is now considered more mature than 3.1.

The important changes are

1. <a href="https://github.com/NICMx/NAT64/issues/106" target="_blank">Jool used to always attempt to mask packets using the first prefix of the pool</a>. This meant that Jool was unable to handle more than one prefix.
2. The <a href="https://github.com/NICMx/NAT64/issues/109" target="_blank">memory leak in the core</a> has been purged.

The less noticeable ones are

1. `log_martians` <a href="https://github.com/NICMx/NAT64/issues/107" target="_blank">is no longer a step</a> in modprobing Jool (though it doesn't bite if you keep it).
2. <a href="https://github.com/NICMx/NAT64/issues/57" target="_blank">The SNMP stat updates returned</a>. See `nstat` and `netstat -s`.
3. Corner-case packets now get <a href="https://github.com/NICMx/NAT64/issues/108" target="_blank">checksums updated correctly</a>.

### 2014-09-01

It took it a really long time to overcome testing, but version 3.2.0 is finally released.

We changed the minor version number this time, because the userspace application has a slightly different interface; the single-value configuration parameters have been joined: [`--general`](usr-flags-global.html) replaced `--filtering`, `--translate` and `--fragmentation`. The application also has three new features:

1. The <a href="https://github.com/NICMx/NAT64/pull/97" target="_blank">ability to flush the pools</a>.
2. The addition of [`--quick`](usr-flags-quick.html).
3. The addition of `--svg`, in [BIB](usr-flags-bib.html#csv) and [session](usr-flags-session.html#csv).

The second main novelty is the finally correct implementation of <a href="https://github.com/NICMx/NAT64/issues/58" target="_blank">Simultaneous Open of TCP Connections</a>. The translation pipeline should now be completely quirkless.

A <a href="https://github.com/NICMx/NAT64/issues/103" target="_blank">little confusion</a> also revealed that the path to libnl <a href="https://github.com/NICMx/NAT64/commit/6455ffd898bae996ce3cab37b2fb6a3459ae096b" target="_blank">used to be hardcoded in the configuration script</a>. If you used to have trouble compiling the userspace application, you might want to try again using the new version.

The more unnoticeable stuff includes a <a href="https://github.com/NICMx/NAT64/issues/100" target="_blank">complement to the old issue #65</a> and a <a href="https://github.com/NICMx/NAT64/issues/56" target="_blank">healthier code-to-comment ratio</a> :). The user documentation, on the other hand, received a significant refactor, so looking at the <a href="https://github.com/NICMx/NAT64/commit/752ed2584534e6bf6bd481d7f4d4ababb6424efe" target="_blank">diff</a> might not be overly productive this time.

One thing we did not complete was the <a href="https://github.com/NICMx/NAT64/issues/104" target="_blank">fragmentation refactor</a>. This is in fact the reason why this milestone dragged. We appear to really need to reconcile the kernel's defragmenter and the RFC in order to implement filtering policies however, so it's still considered an active issue.

We also released 3.1.6, which is small fixes from 3.1.5, in case somebody has a reason to continue using the 3.1.x series.

### 2014-06-26

By the way:

If you can read <a href="https://help.github.com/articles/github-flavored-markdown" target="_blank">Markdown</a> and Github's diffs, you can find the documentation changes for version 3.1.5 <a href="https://github.com/NICMx/NAT64/commit/5295b05cf2c380055c3356d48ef56b74c0b828bb" target="_blank">here</a>, <a href="https://github.com/NICMx/NAT64/commit/2732f520b6616955fb81db778eab9da0f1db210c" target="_blank">here</a> and <a href="https://github.com/NICMx/NAT64/commit/54fc02dd5f5a22c44ac2d6be092306c34abd30ee" target="_blank">here</a>.

### 2014-06-18

Version 3.1.5 released.

Our most important fix is <a href="https://github.com/NICMx/NAT64/issues/92" target="__blank">issue #92</a>. Incorrect ICMP errors used to confuse IPv4 nodes, which lowered the reliability of 4-to-6 traffic.

Aside from that, the userspace application has been tightened. It doesn't crash silly anymore when it has to <a href="https://github.com/NICMx/NAT64/issues/88" target="__blank">output large BIB or session tables</a>, and <a href="https://github.com/NICMx/NAT64/issues/65" target="__blank">works a lot harder to keep the database free from trashy leftover records</a>.

Then we have a couple of <a href="https://github.com/NICMx/NAT64/issues/60" target="__blank">performance</a> <a href="https://github.com/NICMx/NAT64/issues/60" target="__blank">optimizations</a>. In particular (and more or less as a side effect), by aligning log priorities to those from the rest of the kernel, more care has been taken to keep the log cleaner.

If you care about performance, you might want to read the <a href="https://github.com/NICMx/NAT64/issues/91" target="__blank">as-of-now</a>-missing [documentation of `--minMTU6`](misc-mtu.html), a configuration parameter that helps you avoid fragmentation.

If people doesn't find critical bugs in this version, this appears to be the end of the 3.1.x series. We'll go back to aim for 100% RFC compliance in the next update.

### 2014-04-25

Version 3.1.4 released. Fixes:

1. Two <a href="https://github.com/NICMx/NAT64/issues/90" target="_blank">kernel</a> <a href="https://github.com/NICMx/NAT64/issues/84" target="_blank">crashes</a>.
2. The userspace application now <a href="https://github.com/NICMx/NAT64/issues/86" target="_blank">resolves names</a>.
3. <a href="https://github.com/NICMx/NAT64/issues/87" target="_blank">Added support</a> for Linux 3.13+.

Also, we <a href="https://github.com/NICMx/NAT64/issues/90" target="_blank">no longer recommend usage of Jool in kernel 3.12</a>.

### 2014-03-26

Version 3.1.3 released. Fixes:

1. An <a href="https://github.com/NICMx/NAT64/issues/81" target="_blank">incorrect implementation</a> used to ban configuration on certain systems.
2. A <a href="https://github.com/NICMx/NAT64/issues/79" target="_blank">bug</a> used to prevent Jool from sending certain ICMP errors.
3. A <a href="https://github.com/NICMx/NAT64/issues/83" target="_blank">memory leak</a>.
4. Slightly optimized the packet translation algorithm by <a href="https://github.com/NICMx/NAT64/issues/69" target="_blank">replacing some spinlocks with RCUs</a>.

### 2014-03-04

Website released. *This website!*

And with it comes a new release. 3.1.2 fixes:

1. <a href="https://github.com/NICMx/NAT64/issues/76" target="_blank">21-centuried the userspace-app's installation procedure</a>.
2. <a href="https://github.com/NICMx/NAT64/issues/77" target="_blank">Jool is now more explicit regarding the suffix of prefixes</a>.
3. <a href="https://github.com/NICMx/NAT64/issues/78" target="_blank">Jool no longer wrecks itself when modprobed with invalid arguments</a>.

### 2014-02-21

Version 3.1.1 released.

It contains two bugfixes:

1. <a href="https://github.com/NICMx/NAT64/issues/75" target="_blank">Added permission checking to the admin-related userspace requests.</a>
2. <a href="https://github.com/NICMx/NAT64/issues/72" target="_blank">Fixed compatibility issues with ~3.1 kernels.</a>

### 2014-01-15

Version 3.1.0 released. Jool finally handles fragments!

Other important fixes:

* Major optimizations on both the BIB and session databases. The module should scale a lot more gracefully as clients demand more traffic.
* Jool no longer requires a separate IPv4 address.
* Kernel panic during removal of the module fixed.
* And <a href="https://github.com/NICMx/NAT64/issues?milestone=11&state=closed" target="_blank">More stuff</a>.

