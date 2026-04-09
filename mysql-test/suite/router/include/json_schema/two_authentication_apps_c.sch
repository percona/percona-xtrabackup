{
  "type": "array",
  "items": {
    "type": "object",
    "properties": {
      "name": {
        "type": "string",
        "enum": ["c_one-of-two", "c_two-of-two"]
      },
      "vendorId": {
        "type": "string",
        "enum": [
          "0x30000000000000000000000000000000",
          "0x31000000000000000000000000000000"
        ]
      }
    },
    "required": ["name", "vendorId"],
    "additionalProperties": false
  },
  "minItems": 2,
  "maxItems": 2
}