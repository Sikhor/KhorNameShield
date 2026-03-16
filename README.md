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

# Test.bash:
Use test.bash to check for some bad names. It is not in git as the names are very bad and I do not want to appear them somwhere.

# Current REST Adresses:
Currently REST Apis running on 
https://gameinfo.daraempire.com/api/v001/khor-services

# available REST endpoints
/health check if service is up with any webpage via Get
/v1/NameCheck  send {"name": "<NametoCheck>"}" 
/v1/GTSendScore { "score": <score>, "avatarId": "<AvatarId>", "charName": "<Character Name>", "gameId": "<Game Id>"}


