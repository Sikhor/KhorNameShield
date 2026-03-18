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

# Example Response to SendScore
{
  "activeScoreMessages":73,
  "cooldownActive":true,
  "game":{"description":"Harrowed Fields",
  "gameId":"G2",
  "status":"Over",
  "winScore":1000},
  "gameOver":false,
  "gameStatus":"Waiting",
  "hasRunningGame":false,
  "leaderboard":[
      {"avatarId":"A1","charName":"Bernie","totalScore":1080},
      {"avatarId":"A1","charName":"Joinix","totalScore":880},
      {"avatarId":"A1","charName":"Hatari","totalScore":800},
      {"avatarId":"A1","charName":"Trulan","totalScore":720},
      {"avatarId":"A1","charName":"Nanasa","totalScore":640}
      ],
  "nextGameStartsInSeconds":59,
  "ok":true,
  "scoreAccepted":false,
  "scoreAdded":0
}


