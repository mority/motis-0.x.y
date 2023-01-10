import { SchemaTypes } from "../../schema/types";
import { TypeFilter } from "../../filter/type-filter";
import { JSONSchema } from "../json-schema/types";
import { Document } from "yaml";
import { Documentation } from "../../doc/types";

export const OPEN_API_VERSIONS = ["3.0.3", "3.1.0"] as const;

export type OpenAPIVersion = (typeof OPEN_API_VERSIONS)[number];

export interface OpenApiContext {
  schema: SchemaTypes;
  typeFilter: TypeFilter;
  baseUri: URL;
  openApiVersion: OpenAPIVersion;
  jsonSchema: Record<string, JSONSchema>;
  doc: Documentation;
  yd: Document;
  includeIds: boolean;
}
