{
  "type":"object",
  "required":["items","limit","offset","count", "links"],
  "properties": {
     "count" : {"type" : "number", "enum":[0]},
     "offset" : {"type" : "number", "enum":[0]},
     "limit" : {"type" : "number", "minumum":1},
     "hasMore": {"type": "boolean", "enum":[false]},
     "items" : {
        "type" : "array",
        "minItems":0,
        "maxItems":0
     }
  }
}
