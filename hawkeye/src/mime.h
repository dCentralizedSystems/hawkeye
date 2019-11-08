#ifndef __MIME_H
#define __MIME_H

const char *MIME_TYPES[] = {
    "html\0text/html",
    "htm\0text/html",
    "shtml\0text/html",
    "css\0text/css",
    "xml\0text/xml",
    "rss\0text/xml",
    "gif\0image/gif",
    "jpeg\0image/jpeg",
    "jpg\0image/jpeg",
    "js\0application/x-javascript",
    "atom\0application/atom+xml",
    "mml\0text/mathml",
    "txt\0text/plain",
    "jad\0text/vnd.sun.j2me.app-descriptor",
    "wml\0text/vnd.wap.wml",
    "htc\0text/x-component",
    "png\0image/png",
    "tif\0image/tiff",
    "tiff\0image/tiff",
    "wbmp\0image/vnd.wap.wbmp",
    "ico\0image/x-icon",
    "jng\0image/x-jng",
    "bmp\0image/x-ms-bmp",
    "svg\0image/svg+xml",
    "svgz\0image/svg+xml",
    "jar\0application/java-archive",
    "war\0application/java-archive",
    "ear\0application/java-archive",
    "json\0application/json",
    "hqx\0application/mac-binhex40",
    "doc\0application/msword",
    "pdf\0application/pdf",
    "ps\0application/postscript",
    "eps\0application/postscript",
    "ai\0application/postscript",
    "rtf\0application/rtf",
    "xls\0application/vnd.ms-excel",
    "ppt\0application/vnd.ms-powerpoint",
    "wmlc\0application/vnd.wap.wmlc",
    "kml\0application/vnd.google-earth.kml+xml",
    "kmz\0application/vnd.google-earth.kmz",
    "7z\0application/x-7z-compressed",
    "cco\0application/x-cocoa",
    "jardiff\0application/x-java-archive-diff",
    "jnlp\0application/x-java-jnlp-file",
    "run\0application/x-makeself",
    "pl\0application/x-perl",
    "pm\0application/x-perl",
    "prc\0application/x-pilot",
    "pdb\0application/x-pilot",
    "rar\0application/x-rar-compressed",
    "rpm\0application/x-redhat-package-manager",
    "sea\0application/x-sea",
    "swf\0application/x-shockwave-flash",
    "sit\0application/x-stuffit",
    "tcl\0application/x-tcl",
    "tk\0application/x-tcl",
    "der\0application/x-x509-ca-cert",
    "pem\0application/x-x509-ca-cert",
    "crt\0application/x-x509-ca-cert",
    "xpi\0application/x-xpinstall",
    "xhtml\0application/xhtml+xml",
    "zip\0application/zip",
    "bin\0application/octet-stream",
    "exe\0application/octet-stream",
    "dll\0application/octet-stream",
    "deb\0application/octet-stream",
    "dmg\0application/octet-stream",
    "eot\0application/octet-stream",
    "iso\0application/octet-stream",
    "img\0application/octet-stream",
    "msi\0application/octet-stream",
    "msp\0application/octet-stream",
    "msm\0application/octet-stream",
    "ogx\0application/ogg",
    "mid\0audio/midi",
    "midi\0audio/midi",
    "kar\0audio/midi",
    "mpga\0audio/mpeg",
    "mpega\0audio/mpeg",
    "mp2\0audio/mpeg",
    "mp3\0audio/mpeg",
    "m4a\0audio/mpeg",
    "oga\0audio/ogg",
    "ogg\0audio/ogg",
    "spx\0audio/ogg",
    "ra\0audio/x-realaudio",
    "weba\0audio/webm",
    "3gpp\0video/3gpp",
    "3gp\0video/3gpp",
    "mp4\0video/mp4",
    "mpeg\0video/mpeg",
    "mpg\0video/mpeg",
    "mpe\0video/mpeg",
    "ogv\0video/ogg",
    "mov\0video/quicktime",
    "webm\0video/webm",
    "flv\0video/x-flv",
    "mng\0video/x-mng",
    "asx\0video/x-ms-asf",
    "asf\0video/x-ms-asf",
    "wmv\0video/x-ms-wmv",
    "avi\0video/x-msvideo"
};

#define DEFAULT_MIME_TYPE "application/octet-stream"

#endif