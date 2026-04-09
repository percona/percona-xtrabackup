{
  "type":"object",
  "required":["items","limit","offset","count", "links"],
  "properties": {
     "count" : {"type" : "number", "minumum":1},
     "offset" : {"type" : "number", "minumum":0},
     "limit" : {"type" : "number", "minumum":1},
     "hasMore": {"type": "boolean", "enum":[false,true]},
     "items" : {
        "type" : "array",
        "minItems":1,
        "maxItems":25
     }
  }
}
