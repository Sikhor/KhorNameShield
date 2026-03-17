# env should include APIKEY as variable
source .env
BASEURL="https://gameinfo.daraempire.com/api/v001/khor-services/GameTracker/SendScore"

SendScore() {
    local CHARNAME="$1"
    local SCORE=$2

    response=$(curl -sS -X POST\
        -H "Content-Type: application/json" \
        -H "X-Api-Key: $APIKEY" \
        -d "{\"score\":$SCORE,\"avatarId\":\"A1\",\"charName\":\"$CHARNAME\",\"gameId\":\"G1\"}" \
        "$BASEURL")

    # Using jq (recommended)
    ok=$(echo "$response")

    echo "$NAME $ok"
}


SendScore Bernie 120
SendScore Joinix 110
SendScore Hatari 100
SendScore Trulan 90
SendScore Nanasa 80
SendScore Gursyn 70
SendScore Bollie 60
SendScore Jajabi 40
SendScore Bernie 30