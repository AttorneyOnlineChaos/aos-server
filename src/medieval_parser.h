#pragma once

#include <QObject>
#include <QString>

namespace kenji
{
///
/// Medieval text parser, reimplemented from tf_autorp in the Source 1 SDK 2013
/// Please do not report bugs found in this parser without confirming they do not also exist in TF2
///

enum MatchResult
{
  MATCHES_NOT,
  MATCHES_SINGULAR,
  MATCHES_PLURAL
};

class MedievalParser
{
public:
  MedievalParser();

  QString degrootify(const QString &message);

private:
  void parseDataFile();

  struct WordReplacement
  {
    int chance = 1;
    int prepend_count = 0;
    QList<QString> prepended;           // Words that prepend the replacement
    QList<QString> replacements;        // Words that replace the original word
    QList<QString> plural_replacements; // If the match was a plural match, use these replacements instead, if they exist
    QList<QString> words;               // Word that matches this replacement
    QList<QString> plurals;             // Word that must come before to match this replacement, for double word replacements, i.e. "it is" -> "'tis"
    QList<QString> prev_words;          // same ^^
  };

  struct ReplacementCheck
  {
    QString word;
    QString prev_word;
    bool used_prev_word;
  };

  QString getRandomPre();
  QString getRandomPost();
  QString modifySpeech(QString text, bool generate_pre_and_post, bool in_pre_post);
  MatchResult wordMatches(WordReplacement *rep, ReplacementCheck *check);
  bool replaceWord(ReplacementCheck *check, QString *rep, bool symbols, bool word_list_only);
  bool performReplacement(const QString &rep_str, ReplacementCheck *check, QString stored_word, QString *out_text);

  QList<WordReplacement> word_replacements;

  QList<QString> word_vector;

  QList<QString> prepended_words;
  QList<QString> appended_words;

  int prev_pre = 0;
  int prev_post = 0;

  bool datafile_valid;

  int randomInt(int min, int max);
  bool containsCaseInsensitive(const QList<QString> &vector, const QString &str);
};
} // namespace kenji
