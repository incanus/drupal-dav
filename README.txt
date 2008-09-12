// $Id$

DESCRIPTION
-----------
Provides WebDAV access to Drupal's taxonomies and files.

FEATURES
--------
* Unified authentication and access controls: just use your Drupal user name
  and password to login via DAV, too.
* Works against LDAP authentication provided by ldap_integration.module.
* The web display of DAV collection indexes is entirely themeable.

INSTALLATION
------------
In addition to two other needed Drupal modules, this module has PHP version
and PEAR library dependencies. Please refer to the accompanying file
INSTALL.txt for installation requirements and instructions.

IMPORTANT NOTES
---------------
* Do NOT define a URL path alias for 'dav'. It will not work.
* This module has NOT been tested, and may not work, without Clean URLs.

TROUBLESHOOTING
---------------
* Windows's built-in DAV support has numerous serious bugs. If you
  experience problems with Windows 2000 or Windows XP, please refer to:
  http://www.greenbytes.de/tech/webdav/webfolder-client-list.html
  http://www.greenbytes.de/tech/webdav/webdav-redirector-list.html
* If you have trouble mounting the DAV share, try using the bare-bones
  cadaver command line utility to gain a better idea of what's going on:
  http://www.webdav.org/cadaver/
* Alternatively, you can use the cross-platform GUI utility DAV Explorer:
  http://www.davexplorer.org/
* This module includes support for extensively logging information about DAV
  requests provided the Trace module has been installed. If you submit a bug
  report indicating protocol non-compliance or such, expect to be asked to
  submit the Trace log report for a test case. Obtain Trace from:
  http://drupal.org/project/trace

CREDITS
-------
Developed and maintained by Arto Bendiken <http://bendiken.net/>
Development on Drupal 6.x by Justin R. Miller <http://codesorcery.net/>
Sponsored by MakaluMedia Group <http://www.makalumedia.com/>
Sponsored by M.C. Dean, Inc. <http://www.mcdean.com/>
Sponsored by SPAWAR <http://www.spawar.navy.mil/>
PEAR HTTP_WebDAV_Server developed by Hartmut Holzgraefe <hartmut@php.net>
