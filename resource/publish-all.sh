#!/bin/sh

APP_NAME="Whisper-Vamp-Plugins"
PROJECT_URL="https://forge-2.ircam.fr/api/v4/projects/1102/jobs/artifacts"

THIS_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_PATH="$THIS_PATH/.."
PUBLISH_PATH="$REPO_PATH/publish"

while [[ $# -gt 0 ]]; do
  case $1 in
    --tag)
      WVP_VERSION="$2"
      shift
      shift
      ;;
    --token)
      GIT_TOKEN="$2"
      shift
      shift
      ;;
    -u|--user)
      NC_USER="$2"
      shift
      shift
      ;;
    -p|--password)
      NC_PASSWORD="$2"
      shift
      shift
      ;;
    --ci)
      IVP_BUILD_CI=true
      shift # past argument
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      shift # past argument
      ;;
  esac
done

if [ -z $IVP_BUILD_CI ]; then
  if [ -z $GIT_TOKEN ]; then
    echo '\033[0;34m' "Search for a local token..."
    echo '\033[0m'
    GIT_TOKEN=$(security find-generic-password -s 'Forge2 Ircam Token' -w)
  fi

  if [ -z $GIT_TOKEN ]; then
    echo '\033[0;31m' "Error: no private token defined"
    exit -1
  fi

  mkdir -p $PUBLISH_PATH
  cd $PUBLISH_PATH
  rm -rf *

  echo '\033[0;34m' "Preparing " $APP_NAME-v$WVP_VERSION "..."
  echo '\033[0m'

  echo '\033[0;34m' "Downloading MacOS artifact..."
  echo '\033[0m'
  curl --output "$APP_NAME-v$WVP_VERSION-MacOS.zip" --header "PRIVATE-TOKEN: $GIT_TOKEN" "$PROJECT_URL/$WVP_VERSION/download?job=Build::MacOS"

  echo '\033[0;34m' "Downloading Linux artifact..."
  echo '\033[0m'
  curl --output "$APP_NAME-v$WVP_VERSION-Linux.zip" --header "PRIVATE-TOKEN: $GIT_TOKEN" "$PROJECT_URL/$WVP_VERSION/download?job=Build::Linux"

  echo '\033[0;34m' "Downloading Windows artifact..."
  echo '\033[0m'
  curl --output "$APP_NAME-v$WVP_VERSION-Windows.zip" --header "PRIVATE-TOKEN: $GIT_TOKEN" "$PROJECT_URL/$WVP_VERSION/download?job=Build::Windows"

  echo '\033[0;34m' "Downloading Manual artifact..."
  echo '\033[0m'
  curl --output "$APP_NAME-v$WVP_VERSION-Manual.zip" --header "PRIVATE-TOKEN: $GIT_TOKEN" "$PROJECT_URL/$WVP_VERSION/download?job=Doc"

  echo '\033[0;34m' "Installing zip files..."
  echo '\033[0m'
  cp -r $REPO_PATH/publish/ $HOME/Nextcloud/Partiels/Temp/$APP_NAME-v$WVP_VERSION/
else
  if [ -z $NC_USER ]; then
    echo '\033[0;31m' "Error: no private token defined"
    exit -1
  fi

  if [ -z $NC_PASSWORD ]; then
    echo '\033[0;31m' "Error: no private token defined"
    exit -1
  fi

  zip -r "$APP_NAME-v$WVP_VERSION-MacOS.zip" "Whisper.pkg"
  zip -r "$APP_NAME-v$WVP_VERSION-Windows.zip" "Whisper-install.exe"
  zip -r "$APP_NAME-v$WVP_VERSION-Linux.zip" "Whisper"
  zip -r "$APP_NAME-v$WVP_VERSION-Manual.zip" "Whisper-Manual"
  NC_PATH=https://nubo.ircam.fr/remote.php/dav/files/b936dfb8-439c-1038-9400-49253cd64ad0/Partiels/Temp/$APP_NAME-v$WVP_VERSION
  curl -u $NC_USER:$NC_PASSWORD -X MKCOL $NC_PATH
  curl -u $NC_USER:$NC_PASSWORD -T "$APP_NAME-v$WVP_VERSION-MacOS.zip" $NC_PATH/$APP_NAME-v$WVP_VERSION-MacOS.zip
  curl -u $NC_USER:$NC_PASSWORD -T "$APP_NAME-v$WVP_VERSION-Windows.zip" $NC_PATH/$APP_NAME-v$WVP_VERSION-Windows.zip
  curl -u $NC_USER:$NC_PASSWORD -T "$APP_NAME-v$WVP_VERSION-Linux.zip" $NC_PATH/$APP_NAME-v$WVP_VERSION-Linux.zip
  curl -u $NC_USER:$NC_PASSWORD -T "$APP_NAME-v$WVP_VERSION-Manual.zip" $NC_PATH/$APP_NAME-v$WVP_VERSION-Manual.zip
fi

echo '\033[0;34m' "done"
echo '\033[0m'
