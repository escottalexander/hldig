//--------------------------------------------------------------------
//
// ResultFetch.cc
//
// 2/6/2002 created for libhtdig
//
// Neal Richter nealr@rightnow.com
//
// ResultFetch: Takes results of search and fills in the HTML templates
//
//  FOR USE IN LIBHTDIG... does NOT stream to stdout!!
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1995-2003 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: ResultFetch.cc,v 1.3 2003/06/24 19:56:52 nealr Exp $
//
//--------------------------------------------------------------------


#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include "htsearch.h"
#include "ResultFetch.h"
#include "ResultMatch.h"
#include "WeightWord.h"
#include "StringMatch.h"
#include "StringList.h"
#include "QuotedStringList.h"
#include "URL.h"
#include "HtSGMLCodec.h"
#include "HtURLCodec.h"
#include "WordType.h"
#include "Collection.h"
#include "HtURLSeedScore.h"
#include "SplitMatches.h"
#include "HtConfiguration.h"

#include <fstream.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef _WIN32
#include <syslog.h>
#endif

#include <locale.h>
#include <math.h>
#include <float.h>

#if !defined(DBL_MAX) && defined(MAXFLOAT)
# define DBL_MAX MAXFLOAT
#endif

//*****************************************************************************
//
ResultFetch::ResultFetch(Dictionary *collections, const StringList& templist)
//ResultFetch::ResultFetch(Dictionary * collections)
{

    HtConfiguration *config = HtConfiguration::config();
    selected_collections = collections;
    //collectionList = &templist;
    limitTo = 0;
    excludeFrom = 0;
    //    needExcerpt = 0;
    templateError = 0;

    maxStars = config->Value("max_stars");
    maxScore = -DBL_MAX;
    minScore = DBL_MAX;
    setupImages();
    setupTemplates();

    if (!templates.createFromString(config->Find("template_map")))
    {
        // Error in createFromString.
        // Let's try the default template_map

        config->Add("template_map", "Long builtin-long builtin-long Short builtin-short builtin-short");
        if (!templates.createFromString(config->Find("template_map")))
        {
            // Unrecoverable Error
            // (No idea why this would happen)
            templateError = 1;
        }
    }

    currentTemplate = templates.get(config->Find("template_name"));
    if (!currentTemplate)
    {
        //
        // Must have been some error.  Resort to the builtin-long (slot 0)
        //
        currentTemplate = (Template *) templates.templates[0];
    }
    if (!currentTemplate)
    {
        //
        // Another error!? Time to bail out...
        //
        templateError = 1;
    }
    //    if (mystrcasestr(currentTemplate->getMatchTemplate(), "excerpt"))
    //    needExcerpt = 1;
}

//*****************************************************************************
ResultFetch::~ResultFetch()
{
    // docDB.Close();
}

//*****************************************************************************
//
//void
List *
ResultFetch::fetch()
{

    int pageNumber = 1;

    HtConfiguration *config = HtConfiguration::config();
    int good_sort = 0;
    good_sort = ResultMatch::setSortType(config->Find("sort"));
    if (!good_sort)
    {
        // Must temporarily stash the message in a String, since
        // displaySyntaxError will overwrite the static temp used in form.

        String s(form("No such sort method: `%s'", (const char *) config->Find("sort")));

        displaySyntaxError(s);
        //return;
        return(NULL);
    }

    List *matches = buildMatchList();
    //int currentMatch = 0;
    //int numberDisplayed = 0;
    ResultMatch *match = 0;
    int number = 0;
    number = config->Value("matches_per_page");
    if (number <= 0)
        number = 10;
    //int startAt = (pageNumber - 1) * number;

    if (config->Boolean("logging"))
    {
        logSearch(pageNumber, matches);
    }

    setVariables(pageNumber, matches);

    //
    // The first match is guaranteed to have the highest score of
    // all the matches.  We use this to compute the number of stars
    // to display for all the other matches.
    //
    match = (ResultMatch *) (*matches)[0];
    if (!match)
    {
        //
        // No matches.
        //
        delete matches;
        if (config->Boolean("nph"))
        {
            //cout << "HTTP/1.0 200 OK\r\n";
        }
        //cout << "Content-type: text/html\r\n\r\n";

        //displayNomatch();
        //return;
        return(NULL);
    }
    // maxScore = match->getScore();    // now done in buildMatchList()

    if (config->Boolean("nph"))
    {
        //cout << "HTTP/1.0 200 OK\r\n";
    }
    //cout << "Content-type: text/html\r\n\r\n";

    String wrap_file = config->Find("search_results_wrapper");
    String *wrapper = 0;
    char *header = 0, *footer = 0;
    if (wrap_file.length())
    {
        wrapper = readFile(wrap_file.get());
        if (wrapper && wrapper->length())
        {
            char wrap_sepr[] = "HTSEARCH_RESULTS";
            char *h = wrapper->get();
            char *p = strstr(h, wrap_sepr);
            if (p)
            {
                if (p > h && p[-1] == '$')
                {
                    footer = p + strlen(wrap_sepr);
                    header = h;
                    p[-1] = '\0';
                }
                else if (p > h + 1 && p[-2] == '$' &&
                        (p[-1] == '(' || p[-1] == '{') &&
                        (p[strlen(wrap_sepr)] == ')' || p[strlen(wrap_sepr)] == '}'))
                {
                    footer = p + strlen(wrap_sepr) + 1;
                    header = h;
                    p[-2] = '\0';
                }
            }
        }
    }
    if (header)
    {
        //expandVariables(header);
    }
    else
    {
        //displayHeader();
    }


    //neal
    return(matches);

    /*

    //
    // Display the window of matches requested.
    //
    if (!currentTemplate->getStartTemplate().empty())
    {
        expandVariables(currentTemplate->getStartTemplate());
    }

    matches->Start_Get();
    while ((match = (ResultMatch *) matches->Get_Next()) && numberDisplayed < number)
    {
        if (currentMatch >= startAt)
        {
            // DocumentRef  *ref = docDB[match->getID()];
            Collection *collection = match->getCollection();
            DocumentRef *ref = collection->getDocumentRef(match->getID());
            if (!ref || ref->DocState() != Reference_normal)
                continue;      // The document isn't present or shouldn't be displayed
            ref->DocAnchor(match->getAnchor());
            ref->DocScore(match->getScore());
            fetchMatch(match, ref, currentMatch + 1);
            numberDisplayed++;
            delete ref;
        }
        currentMatch++;
    }

    if (!currentTemplate->getEndTemplate().empty())
    {
        expandVariables(currentTemplate->getEndTemplate());
    }
    if (footer)
    {
        //expandVariables(footer);
    }
    else
    {
        //displayFooter();
    }
    
    if (wrapper)
        delete wrapper;
    delete matches;

    */
}

//*****************************************************************************
// Return true if the specified URL should be counted towards the results.
int
ResultFetch::includeURL(const String & url)
{

    if (limitTo && limitTo->match(url, 1, 0) == 0)
        return 0;
    else
    {

        if (excludeFrom && excludeFrom->match(url, 0, 0) != 0)
            return 0;
        else
            return 1;
    }
}

//*****************************************************************************
//void
Dictionary * 
ResultFetch::fetchMatch(ResultMatch * match, DocumentRef * ref, int current)
{
    HtConfiguration *config = HtConfiguration::config();
    String *str = 0;

    char *url = NULL;

    url = form("%s", ref->DocURL());

    vars.Add("URL", new String(url));

    int iA = ref->DocAnchor();

    String *anchor = 0;
    int fanchor = 0;
    if (iA > 0)              // if an anchor was found
    {
        List *anchors = ref->DocAnchors();
        if (anchors->Count() >= iA)
        {
            anchor = new String();
            fanchor = 1;
            *anchor << "#" << ((String *) (*anchors)[iA - 1])->get();
            vars.Add("ANCHOR", anchor);
        }
    }

    //
    // no condition for determining excerpt any more:
    // we need it anyway to see if an anchor is relevant
    //
    int first = -1;
    String urlanchor(url);
    if (anchor)
        urlanchor << anchor;
    vars.Add("EXCERPT", excerpt(match, ref, urlanchor, fanchor, first));
    //
    // anchor only relevant if an excerpt was found, i.e.,
    // the search expression matches the body of the document
    // instead of only META keywords.
    //
    if (first < 0)
    {
        vars.Remove("ANCHOR");
    }

    vars.Add("METADESCRIPTION", new String(ref->DocMetaDsc()));
    vars.Add("SCORE", new String(form("%f", ref->DocScore())));
    vars.Add("CURRENT", new String(form("%d", current)));
    char *title = ref->DocTitle();
    if (!title || !*title)
    {
        if (strcmp(config->Find("no_title_text"), "filename") == 0)
        {
            // use actual file name
            title = strrchr(url, '/');
            if (title)
            {
                title++;      // Skip slash
                str = new String(form("[%s]", title));
            }
            else
                // URL without '/' ??
                str = new String("[No title]");
        }
        else
            // use configure 'no title' text
            str = new String(config->Find("no_title_text"));
    }
    else
        str = new String(title);
    vars.Add("TITLE", str);
    vars.Add("STARSRIGHT", generateStars(ref, 1));
    vars.Add("STARSLEFT", generateStars(ref, 0));
    vars.Add("SIZE", new String(form("%d", ref->DocSize())));
    vars.Add("SIZEK", new String(form("%d", (ref->DocSize() + 1023) / 1024)));

    if (maxScore != 0 && maxScore != minScore)
    {
        int percent = (int) ((ref->DocScore() - minScore) * 100 / (maxScore - minScore));
        if (percent <= 0)
            percent = 1;
        vars.Add("PERCENT", new String(form("%d", percent)));
    }
    else
        vars.Add("PERCENT", new String("100"));

    {
        str = new String();
        char buffer[100];
        time_t t = ref->DocTime();
        if (t)
        {
            struct tm *tm = localtime(&t);
            String datefmt = config->Find("date_format");
            const String locale = config->Find("locale");
            if (datefmt.empty())
            {
                if (config->Boolean("iso_8601"))
                    datefmt = "%Y-%m-%d %H:%M:%S %Z";
                else
                    datefmt = "%x";
            }
            if (!locale.empty())
            {
                setlocale(LC_TIME, locale);
            }
            strftime(buffer, sizeof(buffer), (char *) datefmt, tm);
            *str << buffer;
        }
        vars.Add("MODIFIED", str);
    }

    vars.Add("HOPCOUNT", new String(form("%d", ref->DocHopCount())));
    vars.Add("DOCID", new String(form("%d", ref->DocID())));
    vars.Add("BACKLINKS", new String(form("%d", ref->DocBackLinks())));

    {
        str = new String();
        List *list = ref->Descriptions();
        int n = list->Count();
        for (int i = 0; i < n; i++)
        {
            *str << ((String *) (*list)[i])->get() << "<br>\n";
        }
        vars.Add("DESCRIPTIONS", str);
        String *description = new String();
        if (list->Count())
            *description << ((String *) (*list)[0]);
        vars.Add("DESCRIPTION", description);
    }

    int index = 0;
    int length = 0;
    int status = -1;
    if (URLtemplate.hasPattern())
        status = URLtemplate.FindFirst(ref->DocURL(), index, length);

/*
    if (status >= 0 && index >= 0)
        displayParsedFile(((String *) URLtemplateList[index])->get());
    else
        expandVariables(currentTemplate->getMatchTemplate());



    int vars_count = vars.Count();
    vars.Start_Get();

    String key;
    String * value;

    for(int i = 0; i < vars_count; i++)
    {
        key = vars.Get_Next();
        value = (String *) vars[key];
    
        cout << key.get() << "[" << value->get() << "]" << endl; 
        cout.flush();
    }

*/

    return(&vars);
        
}

//*****************************************************************************
void
ResultFetch::setVariables(int pageNumber, List * matches)
{
    HtConfiguration *config = HtConfiguration::config();
    String tmp;
    int i;
    int nMatches = 0;

    if (matches)
        nMatches = matches->Count();

    int matchesPerPage = config->Value("matches_per_page");
    if (matchesPerPage <= 0)
        matchesPerPage = 10;
    int nPages = (nMatches + matchesPerPage - 1) / matchesPerPage;

    if (nPages > config->Value("maximum_pages", 10))
        nPages = config->Value("maximum_pages", 10);
    if (nPages < 1)
        nPages = 1;          // We always have at least one page...
    vars.Add("MATCHES_PER_PAGE", new String(config->Find("matches_per_page")));
    vars.Add("MAX_STARS", new String(config->Find("max_stars")));
    vars.Add("CONFIG", new String(config->Find("config")));
    vars.Add("VERSION", new String(config->Find("version")));
    vars.Add("RESTRICT", new String(config->Find("restrict")));
    vars.Add("EXCLUDE", new String(config->Find("exclude")));
    vars.Add("KEYWORDS", new String(config->Find("keywords")));
    if (mystrcasecmp(config->Find("match_method"), "and") == 0)
        vars.Add("MATCH_MESSAGE", new String("all"));
    else if (mystrcasecmp(config->Find("match_method"), "or") == 0)
        vars.Add("MATCH_MESSAGE", new String("some"));
    vars.Add("MATCHES", new String(form("%d", nMatches)));
    vars.Add("PLURAL_MATCHES",
            new String((nMatches == 1) ? (char *) "" : (const char *) config->Find("plural_suffix")));
    vars.Add("PAGE", new String(form("%d", pageNumber)));
    vars.Add("PAGES", new String(form("%d", nPages)));
    vars.Add("FIRSTDISPLAYED", new String(form("%d", (pageNumber - 1) * matchesPerPage + 1)));
    if (nPages > 1)
        vars.Add("PAGEHEADER", new String(config->Find("page_list_header")));
    else
        vars.Add("PAGEHEADER", new String(config->Find("no_page_list_header")));

    i = pageNumber * matchesPerPage;
    if (i > nMatches)
        i = nMatches;
    vars.Add("LASTDISPLAYED", new String(form("%d", i)));

    if (config->Find("script_name").length() != 0)
    {
        vars.Add("CGI", new String(config->Find("script_name")));
    }
    else
    {
        vars.Add("CGI", new String(getenv("SCRIPT_NAME")));
    }
    vars.Add("STARTYEAR", new String(config->Find("startyear")));
    vars.Add("STARTMONTH", new String(config->Find("startmonth")));
    vars.Add("STARTDAY", new String(config->Find("startday")));
    vars.Add("ENDYEAR", new String(config->Find("endyear")));
    vars.Add("ENDMONTH", new String(config->Find("endmonth")));
    vars.Add("ENDDAY", new String(config->Find("endday")));

    String *str;
    //char *format = input->get("format");
    char *format = "builtin-long";
    String *in;

    vars.Add("SELECTED_FORMAT", new String(format));

    str = new String();
    *str << "<select name=\"format\">\n";
    for (i = 0; i < templates.displayNames.Count(); i++)
    {
        in = (String *) templates.internalNames[i];
        *str << "<option value=\"" << in->get() << '"';
        if (format && mystrcasecmp(in->get(), format) == 0)
        {
            *str << " selected";
        }
        *str << '>' << ((String *) templates.displayNames[i])->get() << '\n';
    }
    *str << "</select>\n";
    vars.Add("FORMAT", str);

    str = new String();
    QuotedStringList ml(config->Find("method_names"), " \t\r\n");
    *str << "<select name=\"method\">\n";
    for (i = 0; i < ml.Count(); i += 2)
    {
        *str << "<option value=\"" << ml[i] << '"';
        if (mystrcasecmp(ml[i], config->Find("match_method")) == 0)
            *str << " selected";
        *str << '>' << ml[i + 1] << '\n';
    }
    *str << "</select>\n";
    vars.Add("METHOD", str);

    vars.Add("SELECTED_METHOD", new String(config->Find("match_method")));

    ////////////////// Multiple database support //////////////////////
    // Emit collection table. Ensure that previously selected collections
    // are "checked".
    // Collections are specified in the config file with the
    // "collection_names" attribute. An example of the corresponding snippet
    // in the config file is as follows:
    //
    // collection_names: htdig_docs htdig_bugs
    //
    // htdig_bugs and htdig_docs are the two collections (databases) and
    // their corresponding config files are: $CONFIG_DIR/htdig_bugs.conf and
    // $CONFIG_DIR/htdig_docs.conf respectively.
    //
    QuotedStringList clist(config->Find("collection_names"), " \t\r\n");
    for (i = 0; i < clist.Count(); i++)
    {
        String config_name = clist[i];

        for (int j = 0; j < collectionList.Count(); j++)
        {
            if (strcmp(config_name.get(), collectionList[j]) == 0)
            {
                str = new String();
                *str << "checked";
                String collection_id = "COLLECTION_";
                collection_id << config_name;
                vars.Add(collection_id, str);
                break;
            }
        }
    }

    ////////////////// Multiple database support //////////////////////

    str = new String();
    QuotedStringList sl(config->Find("sort_names"), " \t\r\n");
    const String st = config->Find("sort");
    StringMatch datetime;
    datetime.IgnoreCase();
    datetime.Pattern("date|time");
    *str << "<select name=\"sort\">\n";
    for (i = 0; i < sl.Count(); i += 2)
    {
        *str << "<option value=\"" << sl[i] << '"';
        if (mystrcasecmp(sl[i], st) == 0 ||
            datetime.Compare(sl[i]) && datetime.Compare(st) ||
            mystrncasecmp(sl[i], st, 3) == 0 &&
            datetime.Compare(sl[i] + 3) && datetime.Compare(st.get() + 3))
            *str << " selected";
        *str << '>' << sl[i + 1] << '\n';
    }
    *str << "</select>\n";
    vars.Add("SORT", str);
    vars.Add("SELECTED_SORT", new String(st));

    //
    // If a paged output is required, set the appropriate variables
    //
    if (nPages > 1)
    {
        if (pageNumber > 1)
        {
            str = new String("<a href=\"");
            tmp = 0;
            createURL(tmp, pageNumber - 1);
            *str << tmp << "\">" << config->Find("prev_page_text") << "</a>";
        }
        else
        {
            str = new String(config->Find("no_prev_page_text"));
        }
        vars.Add("PREVPAGE", str);

        if (pageNumber < nPages)
        {
            str = new String("<a href=\"");
            tmp = 0;
            createURL(tmp, pageNumber + 1);
            *str << tmp << "\">" << config->Find("next_page_text") << "</a>";
        }
        else
        {
            str = new String(config->Find("no_next_page_text"));
        }
        vars.Add("NEXTPAGE", str);

        str = new String();
        char *p;
        QuotedStringList pnt(config->Find("page_number_text"), " \t\r\n");
        QuotedStringList npnt(config->Find("no_page_number_text"), " \t\r\n");
        QuotedStringList sep(config->Find("page_number_separator"), " \t\r\n");
        if (nPages > config->Value("maximum_page_buttons", 10))
            nPages = config->Value("maximum_page_buttons", 10);
        for (i = 1; i <= nPages; i++)
        {
            if (i == pageNumber)
            {
                p = npnt[i - 1];
                if (!p)
                    p = form("%d", i);
                *str << p;
            }
            else
            {
                p = pnt[i - 1];
                if (!p)
                    p = form("%d", i);
                *str << "<a href=\"";
                tmp = 0;
                createURL(tmp, i);
                *str << tmp << "\">" << p << "</a>";
            }
            if (i != nPages && sep.Count() > 0)
                *str << sep[(i - 1) % sep.Count()];
            else if (i != nPages && sep.Count() <= 0)
                *str << " ";
        }
        vars.Add("PAGELIST", str);
    }
    StringList form_vars(config->Find("allow_in_form"), " \t\r\n");
    String *key;
    for (i = 0; i < form_vars.Count(); i++)
    {
        if (!config->Find(form_vars[i]).empty())
        {
            key = new String(form_vars[i]);
            key->uppercase();
            vars.Add(key->get(), new String(config->Find(form_vars[i])));
        }
    }
}

//*****************************************************************************
void
ResultFetch::createURL(String & url, int pageNumber)
{
    HtConfiguration *config = HtConfiguration::config();
    String s;
    int i;
//#define encodeInput(name) (s = input->get(name), encodeURL(s), s.get())

    if (!config->Find("script_name").empty())
    {
        url << config->Find("script_name");
    }
    else
    {
        url << getenv("SCRIPT_NAME");
    }

    url << '?';

    //if (input->exists("restrict"))
    //    url << "restrict=" << encodeInput("restrict") << '&';
    //if (input->exists("exclude"))
    //    url << "exclude=" << encodeInput("exclude") << '&';
    // Not needed: The next loop below handles this output
    //if (input->exists("config"))
    //    url << "config=" << encodeInput("config") << '&';

    // Put out all specified collections. If none selected, resort to
    // default behaviour.
    char *config_name = collectionList[0];
    String config_encoded;
    if (config_name && config_name[0] == '\0')
        config_name = NULL;

    if (config_name)
    {
        for (i = 0; i < collectionList.Count(); i++)
        {
            config_name = collectionList[i];
            config_encoded = config_name;
            encodeURL(config_encoded);
            url << "config=" << config_encoded << '&';
        }
    }
/*
    if (input->exists("method"))
        url << "method=" << encodeInput("method") << '&';
    if (input->exists("format"))
        url << "format=" << encodeInput("format") << '&';
    if (input->exists("sort"))
        url << "sort=" << encodeInput("sort") << '&';
    if (input->exists("matchesperpage"))
        url << "matchesperpage=" << encodeInput("matchesperpage") << '&';
    if (input->exists("keywords"))
        url << "keywords=" << encodeInput("keywords") << '&';
    if (input->exists("words"))
        url << "words=" << encodeInput("words") << '&';
    if (input->exists("startyear"))
        url << "startyear=" << encodeInput("startyear") << '&';
    if (input->exists("startmonth"))
        url << "startmonth=" << encodeInput("startmonth") << '&';
    if (input->exists("startday"))
        url << "startday=" << encodeInput("startday") << '&';
    if (input->exists("endyear"))
        url << "endyear=" << encodeInput("endyear") << '&';
    if (input->exists("endmonth"))
        url << "endmonth=" << encodeInput("endmonth") << '&';
    if (input->exists("endday"))
        url << "endday=" << encodeInput("endday") << '&';
    StringList form_vars(config->Find("allow_in_form"), " \t\r\n");
    for (i = 0; i < form_vars.Count(); i++)
    {
        if (input->exists(form_vars[i]))
        {
            s = form_vars[i];
            encodeURL(s);      // shouldn't be needed, but just in case
            url << s << '=';
            url << encodeInput(form_vars[i]) << '&';
        }
    }
    url << "page=" << pageNumber;

*/

}

//*****************************************************************************
void
ResultFetch::displayHeader()
{
    HtConfiguration *config = HtConfiguration::config();
    displayParsedFile(config->Find("search_results_header"));
}

//*****************************************************************************
void
ResultFetch::displayFooter()
{
    HtConfiguration *config = HtConfiguration::config();
    displayParsedFile(config->Find("search_results_footer"));
}

//*****************************************************************************
void
ResultFetch::displayNomatch()
{
    HtConfiguration *config = HtConfiguration::config();
    displayParsedFile(config->Find("nothing_found_file"));
}

//*****************************************************************************
void
ResultFetch::displaySyntaxError(const String & message)
{
    HtConfiguration *config = HtConfiguration::config();
    if (config->Boolean("nph"))
    {
    //    cout << "HTTP/1.0 200 OK\r\n";
    }
    //cout << "Content-type: text/html\r\n\r\n";

    setVariables(0, 0);
    vars.Add("SYNTAXERROR", new String(message));
    displayParsedFile(config->Find("syntax_error_file"));
}

//*****************************************************************************
void
ResultFetch::displayParsedFile(const String & filename)
{
    FILE *fl = fopen(filename, "r");
    char buffer[1000];

    while (fl && fgets(buffer, sizeof(buffer), fl))
    {
        expandVariables(buffer);
    }
    if (fl)
        fclose(fl);
    else if (debug)
        cerr << "displayParsedFile: Can't open " << filename << endl;
}

//*****************************************************************************
// If the result templates need to depend on the URL of the match, we need
// an efficient way to determine which template file to use.  To do this, we
// will build a StringMatch object with all the URL patterns and also
// a List parallel to that pattern that contains the actual template file
// names to use for each URL.
//
void
ResultFetch::setupTemplates()
{
    HtConfiguration *config = HtConfiguration::config();
    String templatePatterns = config->Find("template_patterns");
    if (!templatePatterns.empty())
    {
        //
        // The templatePatterns string will have pairs of values.  The first
        // value of a pair will be a pattern, the second value will be a
        // result template file name.
        //
        char *token = strtok(templatePatterns, " \t\r\n");
        String pattern;
        while (token)
        {
            //
            // First token is a pattern...
            //
            pattern << token << '|';

            //
            // Second token is an URL
            //
            token = strtok(0, " \t\r\n");
            URLtemplateList.Add(new String(token));
            if (token)
                token = strtok(0, " \t\r\n");
        }
        pattern.chop(1);
        URLtemplate.Pattern(pattern);
    }
}

//*****************************************************************************
// If the star images need to depend on the URL of the match, we need
// an efficient way to determine which image to use.  To do this, we
// will build a StringMatch object with all the URL patterns and also
// a List parallel to that pattern that contains the actual images to
// use for each URL.
//
void
ResultFetch::setupImages()
{
    HtConfiguration *config = HtConfiguration::config();
    String starPatterns = config->Find("star_patterns");
    if (!starPatterns.empty())
    {
        //
        // The starPatterns string will have pairs of values.  The first
        // value of a pair will be a pattern, the second value will be an
        // URL to an image.
        //
        char *token = strtok(starPatterns, " \t\r\n");
        String pattern;
        while (token)
        {
            //
            // First token is a pattern...
            //
            pattern << token << '|';

            //
            // Second token is an URL
            //
            token = strtok(0, " \t\r\n");
            URLimageList.Add(new String(token));
            if (token)
                token = strtok(0, " \t\r\n");
        }
        pattern.chop(1);
        URLimage.Pattern(pattern);
    }
}

//*****************************************************************************
String *
ResultFetch::generateStars(DocumentRef * ref, int right)
{
    int i;
    String *result = new String();
    HtConfiguration *config = HtConfiguration::config();
    if (!config->Boolean("use_star_image", 1))
        return result;

    String image = config->Find("star_image");
    const String blank = config->Find("star_blank");
    double score;

    if (maxScore != 0 && maxScore != minScore)
    {
        score = (ref->DocScore() - minScore) / (maxScore - minScore);
        if (debug)
            cerr << "generateStars: doc, min, max " << ref->
                DocScore() << ", " << minScore << ", " << maxScore << endl;
    }
    else
    {
        maxScore = ref->DocScore();
        score = 1;
    }
    int nStars = int (score * (maxStars - 1) + 0.5) + 1;

    vars.Add("NSTARS", new String(form("%.d", nStars)));
    if (debug)
        cerr << "generateStars: nStars " << nStars << " of " << maxStars << endl;

    if (right)
    {
        for (i = 0; i < maxStars - nStars; i++)
        {
            *result << "<img src=\"" << blank << "\" alt=\" \">";
        }
    }

    int match = 0;
    int length = 0;
    int status;

    if (URLimage.hasPattern())
        status = URLimage.FindFirst(ref->DocURL(), match, length);
    else
        status = -1;

    if (status >= 0 && match >= 0)
    {
        image = ((String *) URLimageList[match])->get();
    }

    for (i = 0; i < nStars; i++)
    {
        *result << "<img src=\"" << image << "\" alt=\"*\">";
    }

    if (!right)
    {
        for (i = 0; i < maxStars - nStars; i++)
        {
            *result << "<img src=\"" << blank << "\" alt=\" \">";
        }
    }

    return result;
}

//*****************************************************************************
String *
ResultFetch::readFile(const String & filename)
{
    FILE *fl;
    String *s = new String();
    char line[1024];

    fl = fopen(filename, "r");
    while (fl && fgets(line, sizeof(line), fl))
    {
        *s << line;
    }
    if (fl)
        fclose(fl);
    else if (debug)
        cerr << "readFile: Can't open " << filename << endl;
    return s;
}

//*****************************************************************************
void
ResultFetch::expandVariables(const String & str_arg)
{
    const char *str = str_arg;
    enum
    {
        StStart, StLiteral, StVarStart, StVarClose, StVarPlain, StGotVar
    }
    state = StStart;
    String var = "";

    while (str && *str)
    {
        switch (state)
        {
        case StStart:
            if (*str == '\\')
                state = StLiteral;
            else if (*str == '$')
                state = StVarStart;
            else
            {
                cout << *str;
                cout.flush();
            }
            break;
        case StLiteral:
            cout << *str;
            cout.flush();
            state = StStart;
            break;
        case StVarStart:
            if (*str == '%' || *str == '=')
                var << *str;    // code for URL-encoded/decoded variable
            else if (*str == '&')
            {
                var << *str;    // code for SGML-encoded variable
                if (mystrncasecmp("&amp;", str, 5) == 0)
                    str += 4;
            }
            else if (*str == '(' || *str == '{')
                state = StVarClose;
            else if (isalnum(*str) || *str == '_' || *str == '-')
            {
                var << *str;
                state = StVarPlain;
            }
            else
                state = StStart;
            break;
        case StVarClose:
            if (*str == ')' || *str == '}')
                state = StGotVar;
            else if (isalnum(*str) || *str == '_' || *str == '-')
                var << *str;
            else
                state = StStart;
            break;
        case StVarPlain:
            if (isalnum(*str) || *str == '_' || *str == '-')
                var << *str;
            else
            {
                state = StGotVar;
                continue;
            }
            break;
        case StGotVar:
            //
            // We have a complete variable in var. Look it up and
            // see if we can find a good replacement for it.
            //
            outputVariable(var);
            var = "";
            state = StStart;
            continue;
        }
        str++;
    }
    if (state == StGotVar || state == StVarPlain)
    {
        //
        // The end of string was reached, but we are still trying to
        // put a variable together.  Since we now have a complete
        // variable, we will look up the value for it.
        //
        outputVariable(var);
    }
}

//*****************************************************************************
void
ResultFetch::outputVariable(const String & var)
{
    String *temp;
    String value = "";
    const char *ev, *name;

    // We have a complete variable name in var. Look it up and
    // see if we can find a good replacement for it, either in our
    // vars dictionary or in the environment variables.
    name = var;
    while (*name == '&' || *name == '%' || *name == '=')
        name++;
    temp = (String *) vars[name];
    if (temp)
        value = *temp;
    else
    {
        ev = getenv(name);
        if (ev)
            value = ev;
    }
    while (--name >= var.get() && value.length())
    {
        if (*name == '%')
            encodeURL(value);
        else if (*name == '&')
            value = HtSGMLCodec::instance()->decode(value);
        else                  // (*name == '=')
            decodeURL(value);
    }
    cout << value;
    cout.flush();
}

//*****************************************************************************
List *
ResultFetch::buildMatchList()
{
    HtConfiguration *config = HtConfiguration::config();
    char *cpid;
    String url;
    ResultMatch *thisMatch;
    SplitMatches matches(*config);
    double backlink_factor = config->Double("backlink_factor");
    double date_factor = config->Double("date_factor");
    double backlink_score = 0;
    double date_score = 0;
    double base_score = 0;


    // Additions made here by Mike Grommet ...

    tm startdate;              // structure to hold the startdate specified by the user
    tm enddate;              // structure to hold the enddate specified by the user
    time_t now = time((time_t *) 0);    // fill in all fields for mktime
    tm *lt = localtime(&now);  //  - Gilles's fix
    startdate = *lt;
    enddate = *lt;

    time_t eternity = ~(1 << (sizeof(time_t) * 8 - 1));    // will be the largest value holdable by a time_t
    tm *endoftime;              // the time_t eternity will be converted into a tm, held by this variable

    time_t timet_startdate;
    time_t timet_enddate;
    int monthdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    // boolean to test to see if we need to build date information or not
    int dategiven = ((config->Value("startmonth")) ||
                  (config->Value("startday")) ||
                  (config->Value("startyear")) ||
                  (config->Value("endmonth")) || (config->Value("endday")) || (config->Value("endyear")));

    // find the end of time
    endoftime = gmtime(&eternity);

    if (dategiven)              // user specified some sort of date information
    {
        // set up the startdate structure
        // see man mktime for details on the tm structure
        startdate.tm_sec = 0;
        startdate.tm_min = 0;
        startdate.tm_hour = 0;
        startdate.tm_yday = 0;
        startdate.tm_wday = 0;

        // The concept here is that if a user did not specify a part of a date,
        // then we will make assumtions...
        // For instance, suppose the user specified Feb, 1999 as the start
        // range, we take steps to make sure that the search range date starts
        // at Feb 1, 1999,
        // along these same lines:  (these are in MM-DD-YYYY format)
        // Startdates:      Date          Becomes
        //                  01-01         01-01-1970
        //                  01-1970       01-01-1970
        //                  04-1970       04-01-1970
        //                  1970          01-01-1970
        // These things seem to work fine for start dates, as all months have
        // the same first day however the ending date can't work this way.

        if (config->Value("startmonth"))    // form input specified a start month
        {
            startdate.tm_mon = config->Value("startmonth") - 1;
            // tm months are zero based.  They are passed in as 1 based
        }
        else
            startdate.tm_mon = 0;    // otherwise, no start month, default to 0

        if (config->Value("startday"))    // form input specified a start day
        {
            startdate.tm_mday = config->Value("startday");
            // tm days are 1 based, they are passed in as 1 based
        }
        else
            startdate.tm_mday = 1;    // otherwise, no start day, default to 1

        // year is handled a little differently... the tm_year structure
        // wants the tm_year in a format of year - 1900.
        // since we are going to convert these dates to a time_t,
        // a time_t value of zero, the earliest possible date
        // occurs Jan 1, 1970.  If we allow dates < 1970, then we
        // could get negative time_t values right???
        // (barring minor timezone offsets west of GMT, where Epoch is 12-31-69)

        if (config->Value("startyear"))    // form input specified a start year
        {
            startdate.tm_year = config->Value("startyear") - 1900;
            if (startdate.tm_year < 69 - 1900)    // correct for 2-digit years 00-68
                startdate.tm_year += 2000;    //  - Gilles's fix
            if (startdate.tm_year < 0)    // correct for 2-digit years 69-99
                startdate.tm_year += 1900;
        }
        else
            startdate.tm_year = 1970 - 1900;
        // otherwise, no start day, specify start at 1970

        // set up the enddate structure
        enddate.tm_sec = 59;  // allow up to last second of end day
        enddate.tm_min = 59;  //  - Gilles's fix
        enddate.tm_hour = 23;
        enddate.tm_yday = 0;
        enddate.tm_wday = 0;

        if (config->Value("endmonth"))    // form input specified an end month
        {
            enddate.tm_mon = config->Value("endmonth") - 1;
            // tm months are zero based.  They are passed in as 1 based
        }
        else
            enddate.tm_mon = 11;    // otherwise, no end month, default to 11

        if (config->Value("endyear"))    // form input specified a end year
        {
            enddate.tm_year = config->Value("endyear") - 1900;
            if (enddate.tm_year < 69 - 1900)    // correct for 2-digit years 00-68
                enddate.tm_year += 2000;    //  - Gilles's fix
            if (enddate.tm_year < 0)    // correct for 2-digit years 69-99
                enddate.tm_year += 1900;
        }
        else
            enddate.tm_year = endoftime->tm_year;
        // otherwise, no end year, specify end at the end of time allowable

        // Months have different number of days, and this makes things more
        // complicated than the startdate range.
        // Following the example above, here is what we want to happen:
        // Enddates:        Date          Becomes
        //                  04-31         04-31-endoftime->tm_year
        //                  05-1999       05-31-1999, may has 31 days... we want to search until the end of may so...
        //                  1999          12-31-1999, search until the end of the year

        if (config->Value("endday"))    // form input specified an end day
        {
            enddate.tm_mday = config->Value("endday");
            // tm days are 1 based, they are passed in as 1 based
        }
        else
        {
            // otherwise, no end day, default to the end of the month
            enddate.tm_mday = monthdays[enddate.tm_mon];
            if (enddate.tm_mon == 1)    // February, so check for leap year
                if (((enddate.tm_year + 1900) % 4 == 0 &&
                    (enddate.tm_year + 1900) % 100 != 0) || (enddate.tm_year + 1900) % 400 == 0)
                    enddate.tm_mday += 1;    // Feb. 29  - Gilles's fix
        }

        // Convert the tm values into time_t values.
        // Web servers specify modification times in GMT, but htsearch
        // displays these modification times in the server's local time zone.
        // For consistency, we would prefer to select based on this same
        // local time zone.  - Gilles's fix

        timet_startdate = mktime(&startdate);
        timet_enddate = mktime(&enddate);

        // I'm not quite sure what behavior I want to happen if
        // someone reverses the start and end dates, and one of them is invalid.
        // for now, if there is a completely invalid date on the start or end
        // date, I will force the start date to time_t 0, and the end date to
        // the maximum that can be handled by a time_t.

        if (timet_startdate < 0)
            timet_startdate = 0;
        if (timet_enddate < 0)
            timet_enddate = eternity;

        // what if the user did something really goofy like choose an end date
        // that's before the start date

        if (timet_enddate < timet_startdate)    // if so, then swap them so they are in order
        {
            time_t timet_temp = timet_enddate;
            timet_enddate = timet_startdate;
            timet_startdate = timet_temp;
        }
    }
    else                      // no date was specifed, so plug in some defaults
    {
        timet_startdate = 0;
        timet_enddate = eternity;
    }

    // ... MG


    URLSeedScore adjustments(*config);

    // If we knew where to pass it, this would be a good place to pass
    // on errors from adjustments.ErrMsg().

// Deal with all collections
//
    selected_collections->Start_Get();
    Collection *collection = NULL;
    while ((collection = (Collection *) selected_collections->Get_NextElement()))
    {
        ResultList *results = collection->getResultList();
        if (results == NULL)
            continue;

        results->Start_Get();
        while ((cpid = results->Get_Next()))
        {
            int id = atoi(cpid);

            // DocumentRef *thisRef = docDB[id];

            DocMatch *dm = results->find(cpid);
            Collection *collection = NULL;
            if (dm)
                collection = dm->collection;
            if (collection == NULL)
                continue;
            DocumentRef *thisRef = collection->getDocumentRef(id);

            //
            // If it wasn't there, then ignore it
            //
            if (thisRef == 0)
            {
                continue;
            }

            if (!includeURL(thisRef->DocURL()))
            {
                // Get rid of it to free the memory!
                delete thisRef;

                continue;
            }

            // Code added by Mike Grommet for date search ranges
            // check for valid date range.  toss it out if it isn't relevant.
            if ((timet_startdate > 0 || enddate.tm_year < endoftime->tm_year) &&
                (thisRef->DocTime() < timet_startdate || thisRef->DocTime() > timet_enddate))
            {
                delete thisRef;
                continue;
            }

            thisMatch = ResultMatch::create();
            thisMatch->setID(id);
            thisMatch->setCollection(collection);

            //
            // Assign the incomplete score to this match.  This score was
            // computed from the word database only, no excerpt context was
            // known at that time, or info about the document itself, 
            // so this still needs to be done.
            //

            // Moved up: DocMatch    *dm = results->find(cpid);
            double score = dm->score;

            // We need to scale based on date relevance and backlinks
            // Other changes to the score can happen now
            // Or be calculated by the result match in getScore()

            // This formula derived through experimentation
            // We want older docs to have smaller values and the
            // ultimate values to be a reasonable size (max about 100)

            base_score = score;
            if (date_factor != 0.0)
            {
                date_score = date_factor * ((thisRef->DocTime() * 1000.0 / (double) now) - 900);
                score += date_score;
            }

            if (backlink_factor != 0.0)
            {
                int links = thisRef->DocLinks();
                if (links == 0)
                    links = 1;    // It's a hack, but it helps...

                backlink_score = backlink_factor * (thisRef->DocBackLinks() / (double) links);
                score += backlink_score;
            }

            if (debug)
            {
                cerr << thisRef->DocURL() << "\n";
            }

            thisMatch->setTime(thisRef->DocTime());
            thisMatch->setTitle(thisRef->DocTitle());

            score = adjustments.adjust_score(score, thisRef->DocURL());

            // Get rid of it to free the memory!
            delete thisRef;

            score = log(1.0 + score);
            thisMatch->setScore(score);
            thisMatch->setAnchor(dm->anchor);

            //
            // Append this match to our list of matches.
            //
            matches.Add(thisMatch, url.get());

            if (debug)
            {
                cerr << "   base_score " << base_score << " date_score " << date_score <<
                    " backlink_score " << backlink_score << "\n";
                cerr << "   score " << score << "(" << thisMatch->
                    getScore() << "), maxScore " << maxScore << ", minScore " << minScore << endl;
            }

            if (maxScore < score)
            {
                if (debug)
                    cerr << "Set maxScore = score" << endl;
                maxScore = score;
            }
            if (minScore > score)
            {
                if (debug)
                    cerr << "Set minScore = score" << endl;
                minScore = score;
            }
        }
    }

    //
    // Each sub-area is then sorted by relevance level.
    //
    List *matches_part;          // Outside of loop to keep for-scope warnings away.
    for (matches_part = matches.Get_First(); matches_part != 0; matches_part = matches.Get_Next())
        sort(matches_part);

    // Then all sub-lists are concatenated and put in a new list.
    return matches.JoinedLists();
}

//*****************************************************************************
String *
ResultFetch::excerpt(ResultMatch * match, DocumentRef * ref, String urlanchor, int fanchor, int &first)
{
    HtConfiguration *config = HtConfiguration::config();
    // It is necessary to keep alive the String you .get() a char * from,
    // as long as you use the char *.

    //String head_string;

    char *head;
    int use_meta_description = 0;
    Collection *collection = match->getCollection();

    if (config->Boolean("use_meta_description", 0) && strlen(ref->DocMetaDsc()) != 0)
    {
        // Set the head to point to description 
        head = ref->DocMetaDsc();
        use_meta_description = 1;
    }
    else
    {
        // docDB.ReadExcerpt(*ref);
        collection->ReadExcerpt(*ref);
        head = ref->DocHead();    // head points to the top
    }

    //head_string = HtSGMLCodec::instance()->decode(head);
    //head = head_string.get();

    int which, length;
    char *temp = head;
    String part;
    String *text = new String("");

    StringMatch *allWordsPattern = NULL;
    if (collection)
        allWordsPattern = collection->getSearchWordsPattern();
    if (!allWordsPattern)
        return text;

    // htsearch displays the description when:
    // 1) a description has been found
    // 2) the option "use_meta_description" is set to true
    // If previous conditions are false and "excerpt_show_top" is set to true
    // it shows the whole head. Else, it acts as default.

    if (config->Boolean("excerpt_show_top", 0) || use_meta_description || !allWordsPattern->hasPattern())
        first = 0;
    else
        first = allWordsPattern->FindFirstWord(head, which, length);

    if (first < 0 && config->Boolean("no_excerpt_show_top"))
        first = 0;          // No excerpt, but we want to show the top.

    if (first < 0)
    {
        //
        // No excerpt available, don't show top, so display message
        //
        if (!config->Find("no_excerpt_text").empty())
        {
            *text << config->Find("no_excerpt_text");
        }
    }
    else
    {
        int headLength = strlen(head);
        int length = config->Value("excerpt_length", 50);
        char *start;
        char *end;
        WordType type(*config);

        if (!config->Boolean("add_anchors_to_excerpt"))
            // negate flag if it's on (anchor available)
            fanchor = 0;

        //
        // Figure out where to start the excerpt.  Basically we go back
        // half the excerpt length from the first matched word
        //
        start = &temp[first] - length / 2;
        if (start < temp)
            start = temp;
        else
        {
            *text << config->Find("start_ellipses");
            while (*start && type.IsStrictChar(*start))
                start++;
        }

        //
        // Figure out the end of the excerpt.
        //
        end = start + length;
        if (end > temp + headLength)
        {
            end = temp + headLength;
            *text << hilight(match, start, urlanchor, fanchor);
        }
        else
        {
            while (*end && type.IsStrictChar(*end))
                end++;
            *end = '\0';
            *text << hilight(match, start, urlanchor, fanchor);
            *text << config->Find("end_ellipses");
        }
    }
    return text;
}

//*****************************************************************************
String ResultFetch::hilight(ResultMatch * match, const String & str_arg, const String & urlanchor, int fanchor)
{
    HtConfiguration *
        config = HtConfiguration::config();
    const String
        start_highlight = config->Find("start_highlight");
    const String
        end_highlight = config->Find("end_highlight");
    const char *
        str = str_arg;
    String
        result;
    int
        pos = 0;
    int
        which, length;
    WeightWord *
        ww;
    int
        first = 1;
    String
        s;
#define SGMLencodedChars(p, l) (s = 0, s.append(p, l), HtSGMLCodec::instance()->decode(s))

    result = 0;
    Collection *
        collection = match->getCollection();
    StringMatch *
        allWordsPattern = NULL;
    if (collection)
        allWordsPattern = collection->getSearchWordsPattern();
    List *
        searchWords = NULL;
    if (collection)
        searchWords = collection->getSearchWords();
    if (!allWordsPattern || !searchWords)
        return result;

    while (allWordsPattern->hasPattern() && (pos = allWordsPattern->FindFirstWord(str, which, length)) >= 0)
    {
        //result.append(str, pos);
        result << SGMLencodedChars(str, pos);
        ww = (WeightWord *) (*searchWords)[which];
        result << start_highlight;
        if (first && fanchor)
            result << "<a href=\"" << urlanchor << "\">";
        //result.append(str + pos, length);
        result << SGMLencodedChars(str + pos, length);
        if (first && fanchor)
            result << "</a>";
        result << end_highlight;
        str += pos + length;
        first = 0;
    }
    //result.append(str);
    result << SGMLencodedChars(str, strlen(str));
    return result;
}

//*****************************************************************************
void
ResultFetch::sort(List * matches)
{
    HtConfiguration *config = HtConfiguration::config();
    int numberOfMatches = matches->Count();
    int i;

    if (numberOfMatches <= 1)
        return;

    ResultMatch **array = new ResultMatch *[numberOfMatches];
    for (i = 0; i < numberOfMatches; i++)
    {
        array[i] = (ResultMatch *) (*matches)[i];
    }
    matches->Release();

    qsort((char *) array, numberOfMatches, sizeof(ResultMatch *), array[0]->getSortFun());

    const String st = config->Find("sort");
    if (!st.empty() && mystrncasecmp("rev", st, 3) == 0)
    {
        for (i = numberOfMatches; --i >= 0;)
            matches->Add(array[i]);
    }
    else
    {
        for (i = 0; i < numberOfMatches; i++)
            matches->Add(array[i]);
    }
    delete[]array;
}

//*****************************************************************************
void
ResultFetch::logSearch(int page, List * matches)
{
//Note: This is Posix and dependent on a running syslogd..
//does not work for Win32
//TODO: Look into using native windows system logs instead
#ifndef _WIN32

    HtConfiguration *config = HtConfiguration::config();
    // Currently unused    time_t  t;
    int nMatches = 0;
    int level = LOG_LEVEL;
    int facility = LOG_FACILITY;
    char *host = getenv("REMOTE_HOST");
    char *ref = getenv("HTTP_REFERER");

    if (host == NULL)
        host = getenv("REMOTE_ADDR");
    if (host == NULL)
        host = "-";

    if (ref == NULL)
        ref = "-";

    if (matches)
        nMatches = matches->Count();

    openlog("htsearch", LOG_PID, facility);
    syslog(level, "%s [%s] (%s) [%s] [%s] (%d/%s) - %d -- %s\n",
          host,
          input->exists("config") ? input->get("config") : "default",
          (const char *) config->Find("match_method"), input->get("words"), logicalWords.get(),
          nMatches, (const char *) config->Find("matches_per_page"), page, ref);
#endif
}
