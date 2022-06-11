using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

namespace Assets
{
    public class URLParameters : MonoBehaviour
    {
        /*
         * https://some.domain.com:port/path/to/resource/resourceName.html?name=Foobar&id=3#HashString
         * | #1 |  |     #2      | |#3||          #4                     ||      #5       ||    #6   |
         *         |        #7        |
         * |            #8            |
         * |                                          #9                                             |
         * #1: location.protocol  "https:"
         * #2: location.hostname  "some.domain.com"
         * #3: location.port      "port"
         * #4: location.pathname  "/path/to/resource/resourceName.html"
         * #5: location.search    "?name=Foobar&id=3"
         * #6: location.hash      "#HashString"
         * 
         * #7: location.host      "some.domain.com:port"
         * #8: location.origin    "https://some.domain.com:port"
         * #9: location.href      "*full URL*"
        **/

        [System.Serializable]
        public struct TestData
        {
            public string Protocol;
            public string Hostname;
            public string Port;
            public string Pathname;
            public string Search;
            public string Hash;
        }
        public TestData testData;
        public static string Protocol { get { return location_protocol(); } }
        public static string Hostname { get { return location_hostname(); } }
        public static string Port { get { return location_port(); } }
        public static string Pathname { get { return location_pathname(); } }
        public static string Search { get { return location_search(); } set { location_set_search(value); } }
        public static string Hash { get { return location_hash(); } set { location_set_hash(value); } }

        public static string Host { get { return location_host(); } }
        public static string Origin { get { return location_origin(); } }
        public static string Href { get { return location_href(); } }

        private static char[] m_SplitChars = new char[] { '&' };
        private static Dictionary<string, string> ParseURLParams(string aText)
        {
            if (aText == null || aText.Length <= 1)
                return new Dictionary<string, string>();
            // skip "?" / "#" and split parameters at "&"
            var parameters = aText.Substring(1).Split(m_SplitChars);
            var res = new Dictionary<string, string>(parameters.Length);
            foreach (var p in parameters)
            {
                int pos = p.IndexOf('=');
                if (pos > 0)
                    res[p.Substring(0, pos)] = p.Substring(pos + 1);
                else
                    res[p] = "";
            }
            return res;
        }

        public static Dictionary<string, string> GetSearchParameters()
        {
            return ParseURLParams(Search);
        }
        public static Dictionary<string, string> GetHashParameters()
        {
            return ParseURLParams(Hash);
        }
        
#if UNITY_WEBGL && !UNITY_EDITOR
    [DllImport("__Internal")] public static extern string location_protocol();
    [DllImport("__Internal")] public static extern string location_hostname();
    [DllImport("__Internal")] public static extern string location_port();
    [DllImport("__Internal")] public static extern string location_pathname();
    [DllImport("__Internal")] public static extern string location_search();
    [DllImport("__Internal")] public static extern string location_hash();
    [DllImport("__Internal")] public static extern string location_host();
    [DllImport("__Internal")] public static extern string location_origin();
    [DllImport("__Internal")] public static extern string location_href();

    [DllImport("__Internal")] public static extern void location_set_search(string aSearch);
    [DllImport("__Internal")] public static extern void location_set_hash(string aHash);

#else

        private static TestData m_Data;
        public static string location_protocol() { return m_Data.Protocol; }
        public static string location_hostname() { return m_Data.Hostname; }
        public static string location_port() { return m_Data.Port; }
        public static string location_pathname() { return m_Data.Pathname; }
        public static string location_search() { return m_Data.Search; }
        public static string location_hash() { return m_Data.Hash; }
        public static string location_host() { return m_Data.Hostname + (string.IsNullOrEmpty(m_Data.Port) ? "" : (":" + m_Data.Port)); }
        public static string location_origin() { return m_Data.Protocol + "//" + location_host(); }
        public static string location_href() { return location_origin() + m_Data.Pathname + m_Data.Search + m_Data.Hash; }
        public static void location_set_search(string aSearch) { }
        public static void location_set_hash(string aHash) { }

        public void Awake()
        {
            m_Data = testData;
        }

#if UNITY_EDITOR
        private static string m_EmbeddedLib = @"
var URLParamLib = {
    location_protocol: function()
    {
        var str = window.location.protocol;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_hostname: function()
    {
        var str = window.location.hostname;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_port: function()
    {
        var str = window.location.port;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_pathname: function()
    {
        var str = window.location.pathname;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_search: function()
    {
        var str = window.location.search;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_hash: function()
    {
        var str = window.location.hash;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_host: function()
    {
        var str = window.location.host;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_origin: function()
    {
        var str = window.location.origin;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_href: function()
    {
        var str = window.location.href;
        var bufferSize = lengthBytesUTF8(str) + 1;
        var buffer = _malloc(bufferSize);
        stringToUTF8(str, buffer, bufferSize);
        return buffer;
    },
    location_set_search: function(str)
    {
        window.location.search = Pointer_stringify(str);
    },
    location_set_hash: function(str)
    {
        window.location.hash = Pointer_stringify(str);
    },
};
mergeInto(LibraryManager.library, URLParamLib);
";

        [UnityEditor.InitializeOnLoadMethod]
        private static void ExtractJSLib()
        {
            var path = System.IO.Path.Combine(Application.dataPath, "Plugins");
            var folder = new System.IO.DirectoryInfo(path);
            if (!folder.Exists)
                folder.Create();
            path = System.IO.Path.Combine(path, "URLParameters.jslib");
            if (System.IO.File.Exists(path))
                return;
            Debug.Log("URLParameters.jslib does not exist in the plugins folder, extracting");
            System.IO.File.WriteAllText(path, m_EmbeddedLib);
            UnityEditor.AssetDatabase.Refresh();
            UnityEditor.EditorApplication.RepaintProjectWindow();

        }
#endif

#endif

    }

    public static class DictionaryStringStringExt
    {
        public static double GetDouble(this Dictionary<string, string> aDict, string aKey, double aDefault)
        {
            if (!aDict.TryGetValue(aKey, out string str))
                return aDefault;
            if (!double.TryParse(str, NumberStyles.Float, CultureInfo.InvariantCulture, out double val))
                return aDefault;
            return val;
        }

        public static int GetInt(this Dictionary<string, string> aDict, string aKey, int aDefault)
        {
            if (!aDict.TryGetValue(aKey, out string str))
                return aDefault;
            if (!int.TryParse(str, NumberStyles.Integer, CultureInfo.InvariantCulture, out int val))
                return aDefault;
            return val;
        }

        public static string GetString(this Dictionary<string, string> aDict, string aKey, string aDefault)
        {
            if (!aDict.TryGetValue(aKey, out string str))
                return aDefault;
            return str;
        }
    }
}
