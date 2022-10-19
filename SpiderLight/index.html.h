#ifndef _INDEX_HTML_H
#define _INDEX_HTML_H
const char INDEX_HTML[] PROGMEM = R"====(
<html>
<head>
  <title>Spider Light</title>
  <link rel="stylesheet" href="spider-light.css">
  <script src="spider-light-lib.js"></script>
</head>
<body>
  <div id="config"></div>
  <script>
    let spiderLightConfig = new SpiderLightConfig(qs('#config'));
    spiderLightConfig.reload();
  </script>
</body>
</html>
)====";
#endif