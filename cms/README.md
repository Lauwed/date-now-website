## Run app

`cargo build && xdg-open datenowcms://cms.lauradurieux.dev/login\?token=`

## Configuration

`api_url` and `uri_scheme` are hardcoded in `src/data/config.rs`. No env var is
needed: the app must start correctly when the desktop environment launches it
through the `datenowcms://` deep link, where no shell env is available.
