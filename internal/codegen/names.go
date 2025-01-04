package sage

import (
	uid "github.com/rs/xid"
)

func CreateInternalName() string {
	return uid.New().String()
}
