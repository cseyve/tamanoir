<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  
  

  


  

  <head>
    <title>
      /trunk/Palourde/build-clamav.sh –
      Palourde – Trac
    </title>
        <link rel="search" href="/projets/palourde/search" />
        <link rel="help" href="/projets/palourde/wiki/TracGuide" />
        <link rel="alternate" href="/projets/palourde/browser/trunk/Palourde/build-clamav.sh?format=txt" type="text/plain" title="Plain Text" /><link rel="alternate" href="/projets/palourde/export/47/trunk/Palourde/build-clamav.sh" type="application/x-sh; charset=iso-8859-15" title="Original Format" />
        <link rel="up" href="/projets/palourde/browser/trunk/Palourde" title="Parent directory" />
        <link rel="start" href="/projets/palourde/wiki" />
        <link rel="stylesheet" href="/projets/palourde/chrome/common/css/trac.css" type="text/css" /><link rel="stylesheet" href="/projets/palourde/chrome/common/css/code.css" type="text/css" /><link rel="stylesheet" href="/projets/palourde/chrome/common/css/browser.css" type="text/css" />
        <link rel="shortcut icon" href="/projets/palourde/chrome/common/trac.ico" type="image/x-icon" />
        <link rel="icon" href="/projets/palourde/chrome/common/trac.ico" type="image/x-icon" />
      <link type="application/opensearchdescription+xml" rel="search" href="/projets/palourde/search/opensearch" title="Search Palourde" />
    <script type="text/javascript" src="/projets/palourde/chrome/common/js/jquery.js"></script><script type="text/javascript" src="/projets/palourde/chrome/common/js/trac.js"></script><script type="text/javascript" src="/projets/palourde/chrome/common/js/search.js"></script>
    <!--[if lt IE 7]>
    <script type="text/javascript" src="/projets/palourde/chrome/common/js/ie_pre7_hacks.js"></script>
    <![endif]-->
    <script type="text/javascript">
      jQuery(document).ready(function($) {
        $("#jumploc input").hide();
        $("#jumploc select").change(function () {
          this.parentNode.parentNode.submit();
        })
      });
    </script>
  </head>
  <body>
    <div id="banner">
      <div id="header">
        <a id="logo" href="/projets/palourde/wiki/TracIni#header_logo-section"><img src="/projets/palourde/chrome/site/your_project_logo.png" alt="(please configure the [header_logo] section in trac.ini)" /></a>
      </div>
      <form id="search" action="/projets/palourde/search" method="get">
        <div>
          <label for="proj-search">Search:</label>
          <input type="text" id="proj-search" name="q" size="18" value="" />
          <input type="submit" value="Search" />
        </div>
      </form>
      <div id="metanav" class="nav">
    <ul>
      <li class="first"><a href="/projets/palourde/login">Login</a></li><li><a href="/projets/palourde/prefs">Preferences</a></li><li><a href="/projets/palourde/wiki/TracGuide">Help/Guide</a></li><li class="last"><a href="/projets/palourde/about">About Trac</a></li>
    </ul>
  </div>
    </div>
    <div id="mainnav" class="nav">
    <ul>
      <li class="first"><a href="/projets/palourde/wiki">Wiki</a></li><li><a href="/projets/palourde/timeline">Timeline</a></li><li><a href="/projets/palourde/roadmap">Roadmap</a></li><li class="active"><a href="/projets/palourde/browser">Browse Source</a></li><li><a href="/projets/palourde/report">View Tickets</a></li><li class="last"><a href="/projets/palourde/search">Search</a></li>
    </ul>
  </div>
    <div id="main">
      <div id="ctxtnav" class="nav">
        <h2>Context Navigation</h2>
          <ul>
            <li class="first "><a href="/projets/palourde/changeset/47/trunk/Palourde/build-clamav.sh">Last Change</a></li><li><a href="/projets/palourde/browser/trunk/Palourde/build-clamav.sh?annotate=blame&amp;rev=47" title="Annotate each line with the last changed revision (this can be time consuming...)">Annotate</a></li><li class="last"><a href="/projets/palourde/log/trunk/Palourde/build-clamav.sh">Revision Log</a></li>
          </ul>
        <hr />
      </div>
    <div id="content" class="browser">
      <h1>
    <a class="pathentry first" title="Go to root directory" href="/projets/palourde/browser">root</a><span class="pathentry sep">/</span><a class="pathentry" title="View trunk" href="/projets/palourde/browser/trunk">trunk</a><span class="pathentry sep">/</span><a class="pathentry" title="View Palourde" href="/projets/palourde/browser/trunk/Palourde">Palourde</a><span class="pathentry sep">/</span><a class="pathentry" title="View build-clamav.sh" href="/projets/palourde/browser/trunk/Palourde/build-clamav.sh">build-clamav.sh</a>
    <br style="clear: both" />
  </h1>
      <div id="jumprev">
        <form action="" method="get">
          <div>
            <label for="rev">
              View revision:</label>
            <input type="text" id="rev" name="rev" size="6" />
          </div>
        </form>
      </div>
      <div id="jumploc">
        <form action="" method="get">
          <div class="buttons">
            <label for="preselected">Visit:</label>
            <select id="preselected" name="preselected">
              <option selected="selected"></option>
              <optgroup label="branches">
                <option value="/projets/palourde/browser/trunk">trunk</option>
              </optgroup>
            </select>
            <input type="submit" value="Go!" title="Jump to the chosen preselected path" />
          </div>
        </form>
      </div>
      <table id="info" summary="Revision info">
        <tr>
          <th scope="col">
            Revision <a href="/projets/palourde/changeset/47">47</a>, <span title="1377 bytes">1.3 kB</span>
            (checked in by mathieu, <a class="timeline" href="/projets/palourde/timeline?from=2009-03-24T19%3A40%3A40Z%2B0100&amp;precision=second" title="2009-03-24T19:40:40Z+0100 in Timeline">7 days</a> ago)
          </th>
        </tr>
        <tr>
          <td class="message searchable">
              <p>
build for clamav 0.9.5<br />
</p>
          </td>
        </tr>
        <tr>
          <td colspan="2">
            <ul class="props">
              <li>
                  Property <strong>svn:executable</strong> set to
                    <em><code>*</code></em>
              </li>
            </ul>
          </td>
        </tr>
      </table>
      <div id="preview" class="searchable">
    <table class="code"><thead><tr><th class="lineno" title="Line numbers">Line</th><th class="content"> </th></tr></thead><tbody><tr><th id="L1"><a href="#L1">1</a></th><td><b><span class="code-keyword">#!/bin/sh</span></b></td></tr><tr><th id="L2"><a href="#L2">2</a></th><td><b><span class="code-keyword"></span></b></td></tr><tr><th id="L3"><a href="#L3">3</a></th><td>sudo rm -r /Library/Palourde</td></tr><tr><th id="L4"><a href="#L4">4</a></th><td></td></tr><tr><th id="L5"><a href="#L5">5</a></th><td><b><span class="code-lang">export</span></b> CFLAGS='-O0 -isysroot /Developer/SDKs/MacOSX10.5.sdk -arch ppc -arch i386 -mmacosx-version-min=10.5'</td></tr><tr><th id="L6"><a href="#L6">6</a></th><td><b><span class="code-lang">export</span></b> CPPFLAGS='-I/opt/local/include -isysroot /Developer/SDKs/MacOSX10.5.sdk'</td></tr><tr><th id="L7"><a href="#L7">7</a></th><td><b><span class="code-lang">export</span></b> CXXFLAGS='-O2 -isysroot /Developer/SDKs/MacOSX10.5.sdk -arch ppc -arch i386 -mmacosx-version-min=10.5'</td></tr><tr><th id="L8"><a href="#L8">8</a></th><td><b><span class="code-lang">export</span></b> MACOSX_DEPLOYMENT_TARGET='10.5'</td></tr><tr><th id="L9"><a href="#L9">9</a></th><td><b><span class="code-lang">export</span></b> CPP='/usr/bin/cpp-4.0'</td></tr><tr><th id="L10"><a href="#L10">10</a></th><td><b><span class="code-lang">export</span></b> CXX='/usr/bin/g++-4.0'</td></tr><tr><th id="L11"><a href="#L11">11</a></th><td><b><span class="code-lang">export</span></b> F90FLAGS='-O2'</td></tr><tr><th id="L12"><a href="#L12">12</a></th><td><b><span class="code-lang">export</span></b> LDFLAGS='-L/opt/local/lib -arch ppc -arch i386 -mmacosx-version-min=10.5'</td></tr><tr><th id="L13"><a href="#L13">13</a></th><td><b><span class="code-lang">export</span></b> FCFLAGS='-O2'</td></tr><tr><th id="L14"><a href="#L14">14</a></th><td><b><span class="code-lang">export</span></b> OBJC='/usr/bin/gcc-4.0' </td></tr><tr><th id="L15"><a href="#L15">15</a></th><td><b><span class="code-lang">export</span></b> INSTALL='/usr/bin/install -c'</td></tr><tr><th id="L16"><a href="#L16">16</a></th><td><b><span class="code-lang">export</span></b> OBJCFLAGS='-O2' </td></tr><tr><th id="L17"><a href="#L17">17</a></th><td><b><span class="code-lang">export</span></b> FFLAGS='-O2' </td></tr><tr><th id="L18"><a href="#L18">18</a></th><td><b><span class="code-lang">export</span></b> CC='/usr/bin/gcc-4.0'</td></tr><tr><th id="L19"><a href="#L19">19</a></th><td></td></tr><tr><th id="L20"><a href="#L20">20</a></th><td>RESOURCES='/Library/Palourde/Palourde.app/Contents/Resources'</td></tr><tr><th id="L21"><a href="#L21">21</a></th><td>BUILD='build/Release/Palourde.app/Contents/Resources'</td></tr><tr><th id="L22"><a href="#L22">22</a></th><td>PWD=`<b><span class="code-lang">pwd</span></b>`</td></tr><tr><th id="L23"><a href="#L23">23</a></th><td><b><span class="code-lang">cd</span></b> clamav-src &amp;&amp; ./configure --enable-static --with-zlib=/opt/local --with-libbz2-prefix=/opt/local  --with-iconv --prefix=/Library/Palourde/Palourde.app/Contents/Resources --disable-clamav --disable-dependency-tracking --sysconfdir=/Library/Preferences/ --with-dbdir=/Library/Palourde/Definitions/ &amp;&amp; make clean &amp;&amp; make all &amp;&amp; sudo make install</td></tr><tr><th id="L24"><a href="#L24">24</a></th><td><b><span class="code-lang">cd</span></b> ..</td></tr><tr><th id="L25"><a href="#L25">25</a></th><td>mkdir -p $BUILD</td></tr><tr><th id="L26"><a href="#L26">26</a></th><td>cp -rv $RESOURCES/bin $BUILD/</td></tr><tr><th id="L27"><a href="#L27">27</a></th><td>cp -rv $RESOURCES/include $BUILD/</td></tr><tr><th id="L28"><a href="#L28">28</a></th><td>cp -rv $RESOURCES/lib $BUILD/</td></tr><tr><th id="L29"><a href="#L29">29</a></th><td>cp -rv $RESOURCES/sbin $BUILD/</td></tr><tr><th id="L30"><a href="#L30">30</a></th><td>cp -rv /Library/Palourde/Definitions build/Release/</td></tr></tbody></table>
      </div>
      <div id="help">
        <strong>Note:</strong> See <a href="/projets/palourde/wiki/TracBrowser">TracBrowser</a>
        for help on using the browser.
      </div>
      <div id="anydiff">
        <form action="/projets/palourde/diff" method="get">
          <div class="buttons">
            <input type="hidden" name="new_path" value="/trunk/Palourde/build-clamav.sh" />
            <input type="hidden" name="old_path" value="/trunk/Palourde/build-clamav.sh" />
            <input type="hidden" name="new_rev" value="47" />
            <input type="hidden" name="old_rev" value="47" />
            <input type="submit" value="View changes..." title="Select paths and revs for Diff" />
          </div>
        </form>
      </div>
    </div>
    <div id="altlinks">
      <h3>Download in other formats:</h3>
      <ul>
        <li class="first">
          <a rel="nofollow" href="/projets/palourde/browser/trunk/Palourde/build-clamav.sh?format=txt">Plain Text</a>
        </li><li class="last">
          <a rel="nofollow" href="/projets/palourde/export/47/trunk/Palourde/build-clamav.sh">Original Format</a>
        </li>
      </ul>
    </div>
    </div>
    <div id="footer" lang="en" xml:lang="en"><hr />
      <a id="tracpowered" href="http://trac.edgewall.org/"><img src="/projets/palourde/chrome/common/trac_logo_mini.png" height="30" width="107" alt="Trac Powered" /></a>
      <p class="left">
        Powered by <a href="/projets/palourde/about"><strong>Trac 0.11.1</strong></a><br />
        By <a href="http://www.edgewall.org/">Edgewall Software</a>.
      </p>
      <p class="right">Visit the Trac open source project at<br /><a href="http://trac.edgewall.org/">http://trac.edgewall.org/</a></p>
    </div>
  </body>
</html>