## Run app

`cargo build && xdg-open datenowcms://date-now.lauradurieux.dev/login\?token=`

## Export env var

Just using `source .env` is not working as cargo is running in an isolated env.

```bsh
export $(cat .env | xargs)
```
