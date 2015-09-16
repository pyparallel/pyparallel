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
# Aliases
#===============================================================================

#===============================================================================
# Globals/Templates
#===============================================================================
DBNAME = 'geo.db'

#===============================================================================
# Classes
#===============================================================================
class SqliteHttpServer(HttpServer):

    def get_cities(self, request):
        db = sqlite3.connect(DBNAME)
        cur = db.cursor()
        cur.execute("select name from city limit 10");
        results = cur.fetchall()
        #parallel.debugbreak_on_next_exception()
        return json_serialization(request, results)

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
