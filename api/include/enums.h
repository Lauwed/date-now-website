#pragma once

enum http_res_code {
  HTTP_NOT_FOUND = 404,
  HTTP_BAD_REQUEST = 400,
  HTTP_INTERNAL_ERROR = 500
};

enum jwt_type { SUBSCRIPTION = 1, LOGIN = 2, SESSION = 3 };
