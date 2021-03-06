package main

import (
	"net/http"
	"os"

	fmt "github.com/jhunt/go-ansi"
	"github.com/jhunt/go-cli"
	env "github.com/jhunt/go-envirotron"
)

func bail(err error) {
	if err != nil {
		fmt.Fprintf(os.Stderr, "@R{!!! %s}\n", err)
		os.Exit(1)
	}
}

func main() {
	var opt struct {
		Listen       string `cli:"-l, --listen"`
		DB           string `cli:"-d, --db, --database"`
		Endpoint     string `cli:"-e, --endpoint"         env:"BOLO_CORE"`
		WebRoot      string `cli:"-r, --root"`
		SessionLimit int    `cli:"--session-limit"        env:"SESSION_LIMIT"`
	}

	opt.Listen = ":8080"
	opt.Endpoint = "127.0.0.1:2001"
	opt.DB = "api.db"
	opt.WebRoot = "htdocs"
	opt.SessionLimit = 60
	env.Override(&opt)

	_, args, err := cli.Parse(&opt)
	bail(err)

	if len(args) != 0 {
		fmt.Printf("USAGE: bolo-api [-l BIND] [-d DBPATH] [-e IP:PORT]\n")
		os.Exit(1)
	}

	http.Handle("/", http.FileServer(http.Dir(opt.WebRoot)))

	d, err := Database(opt.DB)
	bail(err)

	err = d.Setup()
	bail(err)

	api := &API{
		SessionLimit: opt.SessionLimit * 86400,
		Bolo: &Bolo{
			Endpoint: opt.Endpoint,
		},
		DB: d,
	}
	http.Handle("/v1", api)
	http.Handle("/v1/", api)

	fmt.Printf("@C{listening on %s...}\n", opt.Listen)
	err = http.ListenAndServe(opt.Listen, nil)
	bail(err)
}
