#===============================================================================
# Imports
#===============================================================================
import sqlite3

import parallel

from parallel.http.server import (
    json_serialization,
    HttpServer,
)
#===============================================================================
# Globals
#===============================================================================

#===============================================================================
# Classes
#===============================================================================
class Foo(HttpServer):
    http11 = True
    dbname = 'geo.db'
    def plaintext(self, transport, data):
        return 'Hello, World!\n'

    def json(self, transport, data):
        return { 'message': 'Hello, world!\n' }

    def countries(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select name from country limit 10");
        results = cur.fetchall()
        return results
        #return json_serialization(request, results)

    def cities(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return results

    def country(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from country");
        results = cur.fetchall()
        return results

class SqliteGeoHttpServer(HttpServer):
    #http11 = True
    dbname = 'geo.db'

    def get_countries(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from country");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_cities_limit(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select name from city limit 10");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_cities(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return json_serialization(request, results)

class SqliteWorldHttpServer(HttpServer):
    dbname = 'world.sqlite'

    def get_countries(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from Countries");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_cities(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_city(self, request, name=None):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city ");
        results = cur.fetchall()
        return json_serialization(request, results)

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
