# KhorNameShield

A small REST microservice for validating player/character names:
- format rules (length/charset)
- reserved/impersonation checks
- configurable wordlists from JSON
- leetspeak + homoglyph normalization for bypass resistance

WordLists
Provide your own policy wordlists according to your community guidelines.
A forbidden wordlist must be in the config/lists folder name with extension .json
and should have this format
{
  "category": "hate_speech",
  "severity": "high",
  "matchMode": "contains",
  "words": [
    "example_term1",
    "example_term2"
  ]
}
You can add many wordlist files in the config/lists folder they will all be hotloaded regularly

Service name: **KhorNameShieldService**
