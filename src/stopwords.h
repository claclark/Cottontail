#ifndef COTTONTAIL_SRC_NULL_IDX_H_
#define COTTONTAIL_SRC_NULL_IDX_H_

// Default English stopword list originally developed for TREC CAsT 2019.
// Oriented towards conversational question answering.

namespace cottontail {
std::vector<std::string> cast2019_stopwords = {
    "a",           "about", "an",    "and",  "any",    "are",     "as",
    "at",          "be",    "being", "by",   "can",    "defines", "describe",
    "description", "did",   "do",    "does", "for",    "from",    "give",
    "had",         "has",   "have",  "his",  "how",    "i",       "if",
    "in",          "is",    "isn",   "it",   "its",    "like",    "many",
    "may",         "me",    "much",  "my",   "of",     "on",      "once",
    "one",         "ones",  "or",    "s",    "should", "so",      "some",
    "such",        "t",     "tell",  "than", "that",   "the",     "their",
    "them",        "there", "these", "they", "this",   "to",      "use",
    "using",       "was",   "we",    "well", "were",   "what",    "when",
    "where",       "which", "who",   "why",  "will",   "with",    "you",
    "your",
};
}
#endif // COTTONTAIL_SRC_NULL_IDX_H_
