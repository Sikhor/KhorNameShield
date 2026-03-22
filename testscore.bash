# env should include APIKEY as variable
source .env
SCOREBASEURL="https://gameinfo.daraempire.com/api/v001/khor-services/GameTracker/SendScore"
NEWGAMEBASEURL="https://gameinfo.daraempire.com/api/v001/khor-services/GameTracker/NewGame"

SendScore() {
    local CHARNAME="$1"
    local SCORE=$2
    local AVATARID="$3"
    local BSHOWALLLEADERS="$4"

    response=$(curl -sS -X POST\
        -H "Content-Type: application/json" \
        -H "X-Api-Key: $APIKEY" \
        -d "{\"score\":$SCORE,\"avatarId\":\"$AVATARID\",\"charName\":\"$CHARNAME\"}" \
        "$SCOREBASEURL")
    
    # If third parameter is "true" → print raw JSON
    if [ "$BSHOWALLLEADERS" = "true" ]; then
        echo "$response" | jq -r '
            .leaderboard[]
            | "\(.charName) (\(.totalScore))"
            '
        echo "$response" |jq .
        return
    fi

    # Otherwise: formatted output
    echo "$response" | jq -r '
        if .leaderboard | length > 0 then
            "Leader: \(.leaderboard[0].charName) (\(.leaderboard[0].totalScore)) | Cooldown: \(.cooldownActive) | Next in: \(.nextGameStartsInSeconds)s"
        else
            "No leader | Cooldown: \(.cooldownActive) | Next in: \(.nextGameStartsInSeconds)s"
        end
    '
}



SendScore Bernie 120  1
SendScore Joinix 110  2
SendScore Hatari 100  3
SendScore Trulan 90   4
SendScore Nanasa 80   5
SendScore Gursyn 70   6
SendScore Bollie 60   7
SendScore Jajabi 40   8
SendScore PifPaf 30   9 
SendScore Channi 30   6 true
