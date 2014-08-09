// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class contains common functionality for search-based autocomplete
// providers. Search provider and zero suggest provider both use it for common
// functionality.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/autocomplete/autocomplete_match.h"
#include "components/autocomplete/autocomplete_provider.h"
#include "components/autocomplete/search_suggestion_parser.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "net/url_request/url_fetcher_delegate.h"

class AutocompleteProviderListener;
class GURL;
class Profile;
class SearchTermsData;
class SuggestionDeletionHandler;
class TemplateURL;
class TemplateURLService;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

// Base functionality for receiving suggestions from a search engine.
// This class is abstract and should only be used as a base for other
// autocomplete providers utilizing its functionality.
class BaseSearchProvider : public AutocompleteProvider,
                           public net::URLFetcherDelegate {
 public:
  // ID used in creating URLFetcher for default provider's suggest results.
  static const int kDefaultProviderURLFetcherID;

  // ID used in creating URLFetcher for keyword provider's suggest results.
  static const int kKeywordProviderURLFetcherID;

  // ID used in creating URLFetcher for deleting suggestion results.
  static const int kDeletionURLFetcherID;

  BaseSearchProvider(AutocompleteProviderListener* listener,
                     TemplateURLService* template_url_service,
                     Profile* profile,
                     AutocompleteProvider::Type type);

  // Returns whether |match| is flagged as a query that should be prefetched.
  static bool ShouldPrefetch(const AutocompleteMatch& match);

  // Returns a simpler AutocompleteMatch suitable for persistence like in
  // ShortcutsDatabase.
  // NOTE: Use with care. Most likely you want the other CreateSearchSuggestion
  // with protected access.
  static AutocompleteMatch CreateSearchSuggestion(
      const base::string16& suggestion,
      AutocompleteMatchType::Type type,
      bool from_keyword_provider,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data);

  // AutocompleteProvider:
  virtual void Stop(bool clear_cached_results) OVERRIDE;
  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const OVERRIDE;

  bool field_trial_triggered_in_session() const {
    return field_trial_triggered_in_session_;
  }

 protected:
  // The following keys are used to record additional information on matches.

  // We annotate our AutocompleteMatches with whether their relevance scores
  // were server-provided using this key in the |additional_info| field.
  static const char kRelevanceFromServerKey[];

  // Indicates whether the server said a match should be prefetched.
  static const char kShouldPrefetchKey[];

  // Used to store metadata from the server response, which is needed for
  // prefetching.
  static const char kSuggestMetadataKey[];

  // Used to store a deletion request url for server-provided suggestions.
  static const char kDeletionUrlKey[];

  // These are the values for the above keys.
  static const char kTrue[];
  static const char kFalse[];

  virtual ~BaseSearchProvider();

  typedef std::pair<base::string16, std::string> MatchKey;
  typedef std::map<MatchKey, AutocompleteMatch> MatchMap;
  typedef ScopedVector<SuggestionDeletionHandler> SuggestionDeletionHandlers;

  // Returns an AutocompleteMatch with the given |autocomplete_provider|
  // for the search |suggestion|, which represents a search via |template_url|.
  // If |template_url| is NULL, returns a match with an invalid destination URL.
  //
  // |input| is the original user input. Text in the input is used to highlight
  // portions of the match contents to distinguish locally-typed text from
  // suggested text.
  //
  // |input| is also necessary for various other details, like whether we should
  // allow inline autocompletion and what the transition type should be.
  // |accepted_suggestion| is used to generate Assisted Query Stats.
  // |append_extra_query_params| should be set if |template_url| is the default
  // search engine, so the destination URL will contain any
  // command-line-specified query params.
  static AutocompleteMatch CreateSearchSuggestion(
      AutocompleteProvider* autocomplete_provider,
      const AutocompleteInput& input,
      const SearchSuggestionParser::SuggestResult& suggestion,
      const TemplateURL* template_url,
      const SearchTermsData& search_terms_data,
      int accepted_suggestion,
      bool append_extra_query_params);

  // Returns whether the requirements for requesting zero suggest results
  // are met. The requirements are
  // * The user is enrolled in a zero suggest experiment.
  // * The user is not on the NTP.
  // * The suggest request is sent over HTTPS.  This avoids leaking the current
  //   page URL or personal data in unencrypted network traffic.
  // * The user has suggest enabled in their settings and is not in incognito
  //   mode.  (Incognito disables suggest entirely.)
  // * The user's suggest provider is Google.  We might want to allow other
  //   providers to see this data someday, but for now this has only been
  //   implemented for Google.
  static bool ZeroSuggestEnabled(
     const GURL& suggest_url,
     const TemplateURL* template_url,
     metrics::OmniboxEventProto::PageClassification page_classification,
     const SearchTermsData& search_terms_data,
     Profile* profile);

  // Returns whether we can send the URL of the current page in any suggest
  // requests.  Doing this requires that all the following hold:
  // * ZeroSuggestEnabled() is true, so we meet the requirements above.
  // * The current URL is HTTP, or HTTPS with the same domain as the suggest
  //   server.  Non-HTTP[S] URLs (e.g. FTP/file URLs) may contain sensitive
  //   information.  HTTPS URLs may also contain sensitive information, but if
  //   they're on the same domain as the suggest server, then the relevant
  //   entity could have already seen/logged this data.
  // * The user is OK in principle with sending URLs of current pages to their
  //   provider.  Today, there is no explicit setting that controls this, but if
  //   the user has tab sync enabled and tab sync is unencrypted, then they're
  //   already sending this data to Google for sync purposes.  Thus we use this
  //   setting as a proxy for "it's OK to send such data".  In the future,
  //   especially if we want to support suggest providers other than Google, we
  //   may change this to be a standalone setting or part of some explicit
  //   general opt-in.
  static bool CanSendURL(
      const GURL& current_page_url,
      const GURL& suggest_url,
      const TemplateURL* template_url,
      metrics::OmniboxEventProto::PageClassification page_classification,
      const SearchTermsData& search_terms_data,
      Profile* profile);

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // If the |deletion_url| is valid, then set |match.deletable| to true and
  // save the |deletion_url| into the |match|'s additional info under
  // the key |kDeletionUrlKey|.
  void SetDeletionURL(const std::string& deletion_url,
                      AutocompleteMatch* match);

  // Creates an AutocompleteMatch from |result| to search for the query in
  // |result|. Adds the created match to |map|; if such a match
  // already exists, whichever one has lower relevance is eliminated.
  // |metadata| and |accepted_suggestion| are used for generating an
  // AutocompleteMatch.
  // |mark_as_deletable| indicates whether the match should be marked deletable.
  // NOTE: Any result containing a deletion URL is always marked deletable.
  void AddMatchToMap(const SearchSuggestionParser::SuggestResult& result,
                     const std::string& metadata,
                     int accepted_suggestion,
                     bool mark_as_deletable,
                     MatchMap* map);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists in |results|. |is_keyword_result| indicates
  // whether the response was received from the keyword provider.
  // Returns whether the appropriate result list members were updated.
  bool ParseSuggestResults(const base::Value& root_val,
                           bool is_keyword_result,
                           SearchSuggestionParser::Results* results);

  // Called at the end of ParseSuggestResults to rank the |results|.
  virtual void SortResults(bool is_keyword,
                           SearchSuggestionParser::Results* results);

  // Optionally, cache the received |json_data| and return true if we want
  // to stop processing results at this point. The |parsed_data| is the parsed
  // version of |json_data| used to determine if we received an empty result.
  virtual bool StoreSuggestionResponse(const std::string& json_data,
                                       const base::Value& parsed_data);

  // Returns the TemplateURL corresponding to the keyword or default
  // provider based on the value of |is_keyword|.
  virtual const TemplateURL* GetTemplateURL(bool is_keyword) const = 0;

  // Returns the AutocompleteInput for keyword provider or default provider
  // based on the value of |is_keyword|.
  virtual const AutocompleteInput GetInput(bool is_keyword) const = 0;

  // Returns a pointer to a Results object, which will hold suggest results.
  virtual SearchSuggestionParser::Results* GetResultsToFill(
      bool is_keyword) = 0;

  // Returns whether the destination URL corresponding to the given |result|
  // should contain command-line-specified query params.
  virtual bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const = 0;

  // Stops the suggest query.
  // NOTE: This does not update |done_|.  Callers must do so.
  virtual void StopSuggest() = 0;

  // Clears the current results.
  virtual void ClearAllResults() = 0;

  // Returns the relevance to use if it was not explicitly set by the server.
  virtual int GetDefaultResultRelevance() const = 0;

  // Records in UMA whether the deletion request resulted in success.
  virtual void RecordDeletionResult(bool success) = 0;

  // Records UMA statistics about a suggest server response.
  virtual void LogFetchComplete(bool succeeded, bool is_keyword) = 0;

  // Modify provider-specific UMA statistics.
  virtual void ModifyProviderInfo(
      metrics::OmniboxEventProto_ProviderInfo* provider_info) const;

  // Returns whether the |fetcher| is for the keyword provider.
  virtual bool IsKeywordFetcher(const net::URLFetcher* fetcher) const = 0;

  // Updates |matches_| from the latest results; applies calculated relevances
  // if suggested relevances cause undesriable behavior. Updates |done_|.
  virtual void UpdateMatches() = 0;

  AutocompleteProviderListener* listener_;
  TemplateURLService* template_url_service_;
  Profile* profile_;

  // Whether a field trial, if any, has triggered in the most recent
  // autocomplete query. This field is set to true only if the suggestion
  // provider has completed and the response contained
  // '"google:fieldtrialtriggered":true'.
  bool field_trial_triggered_;

  // Same as above except that it is maintained across the current Omnibox
  // session.
  bool field_trial_triggered_in_session_;

  // The number of suggest results that haven't yet arrived. If it's greater
  // than 0, it indicates that one of the URLFetchers is still running.
  int suggest_results_pending_;

 private:
  friend class SearchProviderTest;
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, TestDeleteMatch);

  // Removes the deleted |match| from the list of |matches_|.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);

  // This gets called when we have requested a suggestion deletion from the
  // server to handle the results of the deletion. It will be called after the
  // deletion request completes.
  void OnDeletionComplete(bool success,
                          SuggestionDeletionHandler* handler);

  // Each deletion handler in this vector corresponds to an outstanding request
  // that a server delete a personalized suggestion. Making this a ScopedVector
  // causes us to auto-cancel all such requests on shutdown.
  SuggestionDeletionHandlers deletion_handlers_;

  DISALLOW_COPY_AND_ASSIGN(BaseSearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_BASE_SEARCH_PROVIDER_H_
