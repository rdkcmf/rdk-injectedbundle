/**
 * Access Control List for Services
 * checks if ServiceManager or some services
 * are allowed to be used in JavaScript context.
 */
var acl = {

    /**
     * Sets ACL in json
     * @return true to notify that was successful
     */
    set: function(json) {
        this._acl = json ? JSON.parse(json) : {};
        return true;
    },

    /**
     * Checks if a service allowed for specific url.
     */
    isServiceAllowed: function(service, url) {
        for (var i in this._acl[service]) {
            var rs = this._acl[service][i];
            var re = new RegExp(rs);
            if (url.search(re) == 0) {
                return true;
            }
        }

        return false;
    },

    /**
     * Checks if there is at least one service allowed
     * for specific url.
     */
    isServiceManagerAllowed: function(url) {
        for (var service in this._acl) {
            if (this.isServiceAllowed(service, url)) {
                return true;
            }
        }

        return false;
    }
};
